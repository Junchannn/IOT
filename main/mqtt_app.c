#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h" 
#include "mqtt_app.h"     

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(client, "esp32/command", 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
}

esp_err_t mqtt_app_start(void)
{
    char broker_url[128] = CONFIG_MQTT_BROKER_URL;
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t url_len = sizeof(broker_url);
        nvs_get_str(nvs_handle, "mqtt_url", broker_url, &url_len);
        nvs_close(nvs_handle);
    }
    
    ESP_LOGI(TAG, "Connecting to MQTT broker: %s", broker_url);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_url,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }
    
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    
    ESP_LOGI(TAG, "MQTT client started");
    return ESP_OK;
}

void mqtt_publish_temperature(int temperature, int humidity)
{
    if (client == NULL) {
        ESP_LOGW(TAG, "MQTT client not initialized");
        return;
    }
    
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"temperature\":%d,\"humidity\":%d}", temperature, humidity);
    
    int msg_id = esp_mqtt_client_publish(client, "esp32/sensor/temperature", payload, 0, 1, 0);
    if (msg_id >= 0) {
        ESP_LOGI(TAG, "Published: %s (msg_id=%d)", payload, msg_id);
    } else {
        ESP_LOGW(TAG, "Failed to publish message");
    }
}
