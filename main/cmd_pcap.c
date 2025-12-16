/* cmd_pcap example - UART output version.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_check.h"
#include "_pcap.h"

static const char *CMD_PCAP_TAG = "cmd_pcap";

#define TRACE_TIMER_FLUSH_INT_MS            (1000)
#define PACKET_START_MARKER                 (0xAAAAAAAA)
#define PACKET_END_MARKER                   (0x55555555)

typedef struct {
    bool is_opened;
    bool is_writing;
    bool link_type_set;
    pcap_file_handle_t pcap_handle;
    pcap_link_type_t link_type;
    TimerHandle_t trace_flush_timer; /*!< Timer handle for stdout buffer flush */
} pcap_runtime_t;

static pcap_runtime_t pcap_rt = {0};

static int uart_writefun(void *cookie, const char *buf, int len)
{
    /* Write binary data to stdout (UART) */
    return fwrite(buf, 1, len, stdout);
}

static int uart_closefun(void *cookie)
{
    /* Flush stdout buffer */
    fflush(stdout);
    return 0;
}

void pcap_flush_uart_timer_cb(TimerHandle_t pxTimer)
{
    /* Periodically flush UART buffer to ensure data is sent */
    fflush(stdout);
}

static esp_err_t pcap_close(pcap_runtime_t *pcap)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(pcap->is_opened, ESP_ERR_INVALID_STATE, err, CMD_PCAP_TAG, ".pcap file is already closed");
    ESP_GOTO_ON_ERROR(pcap_del_session(pcap->pcap_handle) != ESP_OK, err, CMD_PCAP_TAG, "stop pcap session failed");
    pcap->is_opened = false;
    pcap->link_type_set = false;
    pcap->pcap_handle = NULL;
    if (pcap->trace_flush_timer != NULL) {
        xTimerDelete(pcap->trace_flush_timer, pdMS_TO_TICKS(100));
        pcap->trace_flush_timer = NULL;
    }
err:
    return ret;
}

static esp_err_t pcap_open(pcap_runtime_t *pcap)
{
    esp_err_t ret = ESP_OK;
    /* Create file handle to write to UART (stdout) */
    FILE *fp = funopen("uart", NULL, uart_writefun, NULL, uart_closefun);
    ESP_GOTO_ON_FALSE(fp, ESP_FAIL, err, CMD_PCAP_TAG, "open uart stream failed");
    pcap_config_t pcap_config = {
        .fp = fp,
        .major_version = PCAP_DEFAULT_VERSION_MAJOR,
        .minor_version = PCAP_DEFAULT_VERSION_MINOR,
        .time_zone = PCAP_DEFAULT_TIME_ZONE_GMT,
    };
    ESP_GOTO_ON_ERROR(pcap_new_session(&pcap_config, &pcap_rt.pcap_handle), err, CMD_PCAP_TAG, "pcap init failed");
    pcap->is_opened = true;
    ESP_LOGI(CMD_PCAP_TAG, "UART PCAP stream opened successfully");
    return ret;
err:
    if (fp) {
        fclose(fp);
    }
    return ret;
}

esp_err_t packet_capture(void *payload, uint32_t length, uint32_t seconds, uint32_t microseconds)
{
    if (!pcap_rt.is_writing) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Send start marker
    uint32_t start_marker = PACKET_START_MARKER;
    fwrite(&start_marker, sizeof(uint32_t), 1, stdout);
    
    // Send packet length
    fwrite(&length, sizeof(uint32_t), 1, stdout);
    
    // Send PCAP packet record (header + payload)
    esp_err_t ret = pcap_capture_packet(pcap_rt.pcap_handle, payload, length, seconds, microseconds);
    
    // Send end marker
    uint32_t end_marker = PACKET_END_MARKER;
    fwrite(&end_marker, sizeof(uint32_t), 1, stdout);
    
    fflush(stdout);
    
    return ret;
}

esp_err_t sniff_packet_start(pcap_link_type_t link_type)
{
    esp_err_t ret = ESP_OK;

    /* Open UART stream for PCAP output */
    pcap_open(&pcap_rt);

    ESP_GOTO_ON_FALSE(pcap_rt.is_opened, ESP_ERR_INVALID_STATE, err, CMD_PCAP_TAG, "no .pcap file stream is open");
    if (pcap_rt.link_type_set) {
        /* Already initialized - check if we need to update for mixed mode */
        ESP_GOTO_ON_FALSE(!pcap_rt.is_writing, ESP_ERR_INVALID_STATE, err, CMD_PCAP_TAG, "still sniffing");
    } else {
        /* First time initialization - store the link type */
        pcap_rt.link_type = link_type;
        
        /* Create timer for periodic UART buffer flushing */
        int timer_id = 0xFEED;
        pcap_rt.trace_flush_timer = xTimerCreate("flush_uart_timer",
                                    pdMS_TO_TICKS(TRACE_TIMER_FLUSH_INT_MS),
                                    pdTRUE, (void *) timer_id,
                                    pcap_flush_uart_timer_cb);
        ESP_GOTO_ON_FALSE(pcap_rt.trace_flush_timer, ESP_FAIL, err, CMD_PCAP_TAG, "pcap xTimerCreate failed");
        ESP_GOTO_ON_FALSE(xTimerStart(pcap_rt.trace_flush_timer, 0), ESP_FAIL, err_timer_start, CMD_PCAP_TAG, "pcap xTimerStart failed");
        
        pcap_write_header(pcap_rt.pcap_handle, link_type);
        pcap_rt.link_type_set = true;
    }
    pcap_rt.is_writing = true;
    return ret;

err_timer_start:
    xTimerDelete(pcap_rt.trace_flush_timer, pdMS_TO_TICKS(100));
    pcap_rt.trace_flush_timer = NULL;
err:
    return ret;
}

esp_err_t sniff_packet_stop(void)
{
    pcap_rt.is_writing = false;
    pcap_close(&pcap_rt);
    return ESP_OK;
}
