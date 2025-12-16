/* cmd_sniffer example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdlib.h>
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_console.h"
#include "sniffer.h"
#include "_pcap.h"
#include "esp_check.h"
#include "sdkconfig.h"

#define SNIFFER_DEFAULT_CHANNEL             (1)
#define SNIFFER_PAYLOAD_FCS_LEN             (4)
#define SNIFFER_PROCESS_PACKET_TIMEOUT_MS   (100)
#define SNIFFER_RX_FCS_ERR                  (0X41)
#define SNIFFER_MAX_ETH_INTFS               (3)
#define SNIFFER_DECIMAL_NUM                 (10)
#define CONFIG_SNIFFER_WORK_QUEUE_LEN       (64)      // Reduced from 128
#define CONFIG_SNIFFER_TASK_STACK_SIZE      (4096)
#define CONFIG_SNIFFER_TASK_PRIORITY        (2)

// Buffer pool configuration for zero-copy packet handling
#define SNIFFER_BUFFER_POOL_SIZE            (16)      // Pre-allocated buffers
#define SNIFFER_MAX_PACKET_SIZE             (2048)    // Max WiFi packet size


static const char *SNIFFER_TAG = "cmd_sniffer";

typedef struct {
    char *filter_name;
    uint32_t filter_val;
} wlan_filter_table_t;

typedef struct {
    bool is_running;
    bool wifi_enabled;
    bool eth_enabled;
    uint32_t interf_num;
    uint32_t channel;
    uint32_t filter;
    int32_t packets_to_sniff;
    TaskHandle_t task;
    QueueHandle_t work_queue;
    SemaphoreHandle_t sem_task_over;
    esp_eth_handle_t eth_handles[SNIFFER_MAX_ETH_INTFS];
    // Buffer pool for zero-copy packet handling
    uint8_t *buffer_pool[SNIFFER_BUFFER_POOL_SIZE];
    uint32_t buffer_free_mask;  // Bitmap of free buffers
    SemaphoreHandle_t buffer_mutex;
} sniffer_runtime_t;

typedef struct {
    void *payload;
    uint32_t length;
    uint32_t seconds;
    uint32_t microseconds;
} sniffer_packet_info_t;

static sniffer_runtime_t snf_rt = {0};
static esp_err_t sniffer_stop_internal(sniffer_runtime_t *sniffer);

// Buffer pool management functions
static uint8_t* alloc_packet_buffer(void)
{
    uint8_t *buffer = NULL;
    if (xSemaphoreTake(snf_rt.buffer_mutex, 0) == pdTRUE) {
        for (int i = 0; i < SNIFFER_BUFFER_POOL_SIZE; i++) {
            if (snf_rt.buffer_free_mask & (1 << i)) {
                snf_rt.buffer_free_mask &= ~(1 << i);
                buffer = snf_rt.buffer_pool[i];
                break;
            }
        }
        xSemaphoreGive(snf_rt.buffer_mutex);
    }
    return buffer;
}

static void free_packet_buffer(uint8_t *buffer)
{
    if (xSemaphoreTake(snf_rt.buffer_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < SNIFFER_BUFFER_POOL_SIZE; i++) {
            if (snf_rt.buffer_pool[i] == buffer) {
                snf_rt.buffer_free_mask |= (1 << i);
                break;
            }
        }
        xSemaphoreGive(snf_rt.buffer_mutex);
    }
}

static void queue_packet(void *recv_packet, sniffer_packet_info_t *packet_info)
{
    /* Use pre-allocated buffer pool instead of malloc for better performance */
    void *packet_to_queue = alloc_packet_buffer();
    if (packet_to_queue) {
        memcpy(packet_to_queue, recv_packet, packet_info->length);
        packet_info->payload = packet_to_queue;
        if (snf_rt.work_queue) {
            /* send packet_info */
            if (xQueueSend(snf_rt.work_queue, packet_info, 0) != pdTRUE) {
                // Queue full - drop packet and return buffer to pool
                free_packet_buffer(packet_to_queue);
            }
        }
    }
    // If no buffer available, packet is dropped silently (no malloc fallback to reduce overhead)
}

static void wifi_sniffer_cb(void *recv_buf, wifi_promiscuous_pkt_type_t type)
{
    sniffer_packet_info_t packet_info;
    wifi_promiscuous_pkt_t *sniffer = (wifi_promiscuous_pkt_t *)recv_buf;
    /* prepare packet_info */
    packet_info.seconds = sniffer->rx_ctrl.timestamp / 1000000U;
    packet_info.microseconds = sniffer->rx_ctrl.timestamp % 1000000U;
    packet_info.length = sniffer->rx_ctrl.sig_len;

    /* For now, the sniffer only dumps the length of the MISC type frame */
    if (type != WIFI_PKT_MISC && !sniffer->rx_ctrl.rx_state) {
        packet_info.length -= SNIFFER_PAYLOAD_FCS_LEN;
        queue_packet(sniffer->payload, &packet_info);
    }
}

static esp_err_t eth_sniffer_cb(esp_eth_handle_t eth_handle, uint8_t *buffer, uint32_t length, void *priv)
{
    sniffer_packet_info_t packet_info;
    struct timeval tv_now;

    // ESP32 Ethernet MAC provides hardware time stamping for incoming frames in its Linked List Descriptors (see TMR, section 10.8.2).
    // However, this information is not currently accessible via Ethernet driver => do at least software time stamping
    gettimeofday(&tv_now, NULL);

    packet_info.seconds = tv_now.tv_sec;
    packet_info.microseconds = tv_now.tv_usec;
    packet_info.length = length;

    queue_packet(buffer, &packet_info);

    free(buffer);

    return ESP_OK;
}

static void sniffer_task(void *parameters)
{
    sniffer_packet_info_t packet_info;
    sniffer_runtime_t *sniffer = (sniffer_runtime_t *)parameters;

    while (sniffer->is_running) {
        if (sniffer->packets_to_sniff == 0) {
            sniffer_stop_internal(sniffer);
            break;
        }
        /* receive packet info from queue */
        if (xQueueReceive(sniffer->work_queue, &packet_info, pdMS_TO_TICKS(SNIFFER_PROCESS_PACKET_TIMEOUT_MS)) != pdTRUE) {
            continue;
        }
        if (packet_capture(packet_info.payload, packet_info.length, packet_info.seconds,
                           packet_info.microseconds) != ESP_OK) {
            ESP_LOGW(SNIFFER_TAG, "save captured packet failed");
        }
        free_packet_buffer(packet_info.payload);  // Return buffer to pool
        if (sniffer->packets_to_sniff > 0) {
            sniffer->packets_to_sniff--;
        }

    }
    /* notify that sniffer task is over */
    if (sniffer->packets_to_sniff != 0) {
        xSemaphoreGive(sniffer->sem_task_over);
    }
    vTaskDelete(NULL);
}

static esp_err_t sniffer_stop_internal(sniffer_runtime_t *sniffer)
{
    bool eth_set_promiscuous;
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(sniffer->is_running, ESP_ERR_INVALID_STATE, err, SNIFFER_TAG, "sniffer is already stopped");

    /* Disable WiFi promiscuous mode if it was enabled */
    if (sniffer->wifi_enabled) {
        ESP_GOTO_ON_ERROR(esp_wifi_set_promiscuous(false), err, SNIFFER_TAG, "stop wifi promiscuous failed");
        ESP_LOGI(SNIFFER_TAG, "WiFi promiscuous stopped");
        sniffer->wifi_enabled = false;
    }

    /* Disable Ethernet Promiscuous Mode if it was enabled */
    if (sniffer->eth_enabled) {
        eth_set_promiscuous = false;
        ESP_GOTO_ON_ERROR(esp_eth_ioctl(sniffer->eth_handles[sniffer->interf_num], ETH_CMD_S_PROMISCUOUS, &eth_set_promiscuous),
                          err, SNIFFER_TAG, "stop Ethernet promiscuous failed");
        esp_eth_update_input_path(sniffer->eth_handles[sniffer->interf_num], NULL, NULL);
        ESP_LOGI(SNIFFER_TAG, "Ethernet promiscuous stopped");
        sniffer->eth_enabled = false;
    }

    /* stop sniffer local task */
    sniffer->is_running = false;
    /* wait for task over */
    if (sniffer->packets_to_sniff != 0) {
        xSemaphoreTake(sniffer->sem_task_over, portMAX_DELAY);
    }

    vSemaphoreDelete(sniffer->sem_task_over);
    sniffer->sem_task_over = NULL;
    /* make sure to free all resources in the left items */
    UBaseType_t left_items = uxQueueMessagesWaiting(sniffer->work_queue);

    sniffer_packet_info_t packet_info;
    while (left_items--) {
        xQueueReceive(sniffer->work_queue, &packet_info, pdMS_TO_TICKS(SNIFFER_PROCESS_PACKET_TIMEOUT_MS));
        free_packet_buffer(packet_info.payload);  // Return buffer to pool
    }
    vQueueDelete(sniffer->work_queue);
    sniffer->work_queue = NULL;

    /* stop pcap session */
    sniff_packet_stop();
err:
    return ret;
}

static esp_err_t sniffer_start(sniffer_runtime_t *sniffer)
{
    esp_err_t ret = ESP_OK;
    wifi_promiscuous_filter_t wifi_filter;
    bool eth_set_promiscuous;

    ESP_GOTO_ON_FALSE(!(sniffer->is_running), ESP_ERR_INVALID_STATE, err, SNIFFER_TAG, "sniffer is already running");

    /* Initialize buffer pool with DMA-capable memory */
    sniffer->buffer_mutex = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(sniffer->buffer_mutex, ESP_FAIL, err, SNIFFER_TAG, "create buffer mutex failed");
    
    sniffer->buffer_free_mask = 0xFFFFFFFF;  // All buffers initially free
    for (int i = 0; i < SNIFFER_BUFFER_POOL_SIZE; i++) {
        sniffer->buffer_pool[i] = heap_caps_malloc(SNIFFER_MAX_PACKET_SIZE, MALLOC_CAP_DMA);
        if (!sniffer->buffer_pool[i]) {
            ESP_LOGE(SNIFFER_TAG, "Failed to allocate DMA buffer %d", i);
            // Cleanup already allocated buffers
            for (int j = 0; j < i; j++) {
                heap_caps_free(sniffer->buffer_pool[j]);
            }
            vSemaphoreDelete(sniffer->buffer_mutex);
            ret = ESP_FAIL;
            goto err;
        }
    }
    ESP_LOGI(SNIFFER_TAG, "Allocated %d DMA buffers (%d KB total)", 
             SNIFFER_BUFFER_POOL_SIZE, (SNIFFER_BUFFER_POOL_SIZE * SNIFFER_MAX_PACKET_SIZE) / 1024);

    /* init pcap session for WiFi (802.11) - will be used for both interfaces */
    ESP_GOTO_ON_ERROR(sniff_packet_start(PCAP_LINK_TYPE_802_11), err_buffers, SNIFFER_TAG, "init pcap session failed");

    sniffer->is_running = true;
    sniffer->work_queue = xQueueCreate(CONFIG_SNIFFER_WORK_QUEUE_LEN, sizeof(sniffer_packet_info_t));
    ESP_GOTO_ON_FALSE(sniffer->work_queue, ESP_FAIL, err_queue, SNIFFER_TAG, "create work queue failed");
    sniffer->sem_task_over = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(sniffer->sem_task_over, ESP_FAIL, err_sem, SNIFFER_TAG, "create work queue failed");
    ESP_GOTO_ON_FALSE(xTaskCreate(sniffer_task, "snifferT", CONFIG_SNIFFER_TASK_STACK_SIZE,
                                  sniffer, CONFIG_SNIFFER_TASK_PRIORITY, &sniffer->task), ESP_FAIL,
                      err_task, SNIFFER_TAG, "create task failed");

    /* Start WiFi Promiscuous Mode */
    wifi_filter.filter_mask = sniffer->filter;
    esp_wifi_set_promiscuous_filter(&wifi_filter);
    esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb);
    if (esp_wifi_set_promiscuous(true) == ESP_OK) {
        esp_wifi_set_channel(sniffer->channel, WIFI_SECOND_CHAN_NONE);
        sniffer->wifi_enabled = true;
        ESP_LOGI(SNIFFER_TAG, "WiFi promiscuous started on channel %d", sniffer->channel);
    } else {
        ESP_LOGW(SNIFFER_TAG, "Failed to start WiFi promiscuous mode");
    }
    
    /* Start Ethernet Promiscuous Mode if Ethernet handle is available */
    if (sniffer->eth_handles[sniffer->interf_num] != NULL) {
        eth_set_promiscuous = true;
        if (esp_eth_ioctl(sniffer->eth_handles[sniffer->interf_num], ETH_CMD_S_PROMISCUOUS, &eth_set_promiscuous) == ESP_OK) {
            esp_eth_update_input_path(sniffer->eth_handles[sniffer->interf_num], eth_sniffer_cb, NULL);
            sniffer->eth_enabled = true;
            ESP_LOGI(SNIFFER_TAG, "Ethernet promiscuous started");
        } else {
            ESP_LOGW(SNIFFER_TAG, "Failed to start Ethernet promiscuous mode");
        }
    }

    /* Check if at least one interface was successfully started */
    if (!sniffer->wifi_enabled && !sniffer->eth_enabled) {
        ESP_LOGE(SNIFFER_TAG, "No interfaces enabled for sniffing");
        ret = ESP_FAIL;
        goto err_start;
    }
        
    return ret;
err_start:
    vTaskDelete(sniffer->task);
    sniffer->task = NULL;
err_task:
    vSemaphoreDelete(sniffer->sem_task_over);
    sniffer->sem_task_over = NULL;
err_sem:
    vQueueDelete(sniffer->work_queue);
    sniffer->work_queue = NULL;
err_queue:
    sniffer->is_running = false;
err_buffers:
    // Cleanup buffer pool
    for (int i = 0; i < SNIFFER_BUFFER_POOL_SIZE; i++) {
        if (sniffer->buffer_pool[i]) {
            heap_caps_free(sniffer->buffer_pool[i]);
            sniffer->buffer_pool[i] = NULL;
        }
    }
    if (sniffer->buffer_mutex) {
        vSemaphoreDelete(sniffer->buffer_mutex);
        sniffer->buffer_mutex = NULL;
    }
err:
    return ret;
}


esp_err_t sniffer_reg_eth_intf(esp_eth_handle_t eth_handle)
{
    esp_err_t ret = ESP_OK;
    int32_t i = 0;
    while ((snf_rt.eth_handles[i] != NULL) && (i < SNIFFER_MAX_ETH_INTFS)) {
        i++;
    }
    ESP_GOTO_ON_FALSE(i < SNIFFER_MAX_ETH_INTFS, ESP_FAIL, err, SNIFFER_TAG, "maximum num. of eth interfaces registered");
    snf_rt.eth_handles[i] = eth_handle;

err:
    return ret;
}


esp_err_t sniffer_init(uint32_t channel, int32_t num_packets)
{
    /* Set WiFi channel (1-13) */
    snf_rt.channel = (channel >= 1 && channel <= 13) ? channel : SNIFFER_DEFAULT_CHANNEL;
    
    /* Set WiFi filter to capture all packet types */
    snf_rt.filter = WIFI_PROMIS_FILTER_MASK_ALL;
    
    /* Set number of packets to capture (-1 = unlimited) */
    snf_rt.packets_to_sniff = num_packets;
    
    /* Interface number for Ethernet (0 = first interface) */
    snf_rt.interf_num = 0;
    
    /* Start sniffer for both WiFi and Ethernet */
    return sniffer_start(&snf_rt);
}


esp_err_t sniffer_stop(void)
{
    return sniffer_stop_internal(&snf_rt);
}

/* Note: Ethernet initialization should be done separately in main.c
 * using esp_eth_* APIs and then registered via sniffer_reg_eth_intf() */