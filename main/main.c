#include <stdio.h>
#include "nvs_flash.h"
#include "dht11.h"
#include "buzzer.h"
#include "wifi_manager.h"
#include "mqtt_app.h"
// #include "coap_app.h"
#include "sniffer.h"

static const char *TAG = "MAIN";

void test1(){
    ESP_LOGI(TAG, "Starting ESP32 Temperature Monitor");
    
    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(DHT_TAG, "DHT11 Temperature Monitor with WiFi/MQTT");
    ESP_LOGI(BUZZER_TAG, "Alert threshold: 27°C");
    
    // Configure DHT11 GPIO with pull-up
    gpio_config_t dht_conf = {
        .pin_bit_mask = 1ULL << DHT_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&dht_conf);
    gpio_set_level(DHT_GPIO, 1);
    
    // Configure Buzzer GPIO
    gpio_config_t buzzer_conf = {
        .pin_bit_mask = 1ULL << BUZZER_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&buzzer_conf);
    gpio_set_level(BUZZER_GPIO, 0);
    gpio_set_drive_capability(BUZZER_GPIO, GPIO_DRIVE_CAP_3);
    
    // Test buzzer
    ESP_LOGI(BUZZER_TAG, "Testing buzzer...");
    buzzer_beep(2500, 200);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Initialize DHT11
    ESP_LOGI(TAG, "Initializing DHT11 sensor...");
    DHT11_init(DHT_GPIO);
    
    // Connect to WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    if (wifi_init_sta() == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected successfully");
        
        // Start CoAP server
        // ESP_LOGI(TAG, "Starting CoAP server...");
        // coap_server_start();
        
        // Start MQTT
        ESP_LOGI(TAG, "Starting MQTT client...");
        mqtt_app_start();
        vTaskDelay(pdMS_TO_TICKS(2000));  // Wait for MQTT to connect
        
        // Start packet sniffer on WiFi channel 1
        // ESP_LOGI(TAG, "Starting packet sniffer...");
        // sniffer_init(1, -1);  // Channel 1, unlimited packets
    } else {
        ESP_LOGW(TAG, "WiFi connection failed. Running in offline mode.");
    }

    while (1) {
        struct dht11_reading reading = DHT11_read();

        if (reading.status == DHT11_OK) {
            ESP_LOGI(DHT_TAG, "Temperature: %d °C, Humidity: %d %%", reading.temperature, reading.humidity);
            
            // Update CoAP server data
            // coap_update_temperature(reading.temperature, reading.humidity);
            
            // Publish to MQTT if connected
            if (wifi_is_connected()) {
                mqtt_publish_temperature(reading.temperature, reading.humidity);
            }
            
            // Check if temperature exceeds 27°C
            if (reading.temperature > 27) {
                ESP_LOGW(DHT_TAG, "⚠️  HIGH TEMPERATURE ALERT! ⚠️");
                buzzer_beep(2500, 300);
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_beep(3000, 300);
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_beep(2500, 300);
            } else {
                buzzer_off();
            }
        } else if (reading.status == DHT11_TIMEOUT_ERROR) {
            ESP_LOGW(DHT_TAG, "DHT11 timeout error - sensor not responding");
        } else if (reading.status == DHT11_CRC_ERROR) {
            ESP_LOGW(DHT_TAG, "DHT11 CRC error - data corrupted");
        } else {
            ESP_LOGW(DHT_TAG, "DHT11 unknown error (status: %d)", reading.status);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));    // read every 5 seconds
    }
}

// void test_coap(){
//     ESP_LOGI(TAG, "Starting CoAP server test");
//     // Start CoAP server
//     ESP_LOGI(TAG, "Starting CoAP server...");
//     coap_server_start();
// }
void app_main(void){
    // Call test1() which includes MQTT functionality
    test1();
}
