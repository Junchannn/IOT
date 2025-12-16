#include "coap_app.h"

static const char *TAG = "CoAP";
static coap_context_t *coap_context = NULL;

// Shared temperature data
static int current_temperature = 0;
static int current_humidity = 0;

void coap_update_temperature(int temperature, int humidity)
{
    current_temperature = temperature;
    current_humidity = humidity;
}

/* GET handler for /temperature resource */
static void coap_temperature_handler(coap_resource_t *resource,
                                     coap_session_t *session,
                                     const coap_pdu_t *request,
                                     const coap_string_t *query,
                                     coap_pdu_t *response)
{
    ESP_LOGI(TAG, "GET request received for /temperature");
    
    // Return current temperature and humidity
    char temp_data[64];
    snprintf(temp_data, sizeof(temp_data), "{\"temperature\":%d,\"humidity\":%d}", 
             current_temperature, current_humidity);
    
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(resource, session, request, response,
                                  query, COAP_MEDIATYPE_APPLICATION_JSON,
                                  -1, 0, strlen(temp_data),
                                  (const uint8_t *)temp_data,
                                  NULL, NULL);
}

static void coap_server_task(void *pvParameters)
{
    coap_address_t serv_addr;
    coap_resource_t *temperature_resource;
    
    // Initialize CoAP context
    coap_context = coap_new_context(NULL);
    if (!coap_context) {
        ESP_LOGE(TAG, "Failed to create CoAP context");
        vTaskDelete(NULL);
        return;
    }
    
    // Set up server address (bind to all interfaces on port 5683)
    coap_address_init(&serv_addr);
    serv_addr.addr.sin.sin_family = AF_INET;
    serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
    serv_addr.addr.sin.sin_port = htons(COAP_DEFAULT_PORT);
    
    // Create endpoint
    coap_endpoint_t *endpoint = coap_new_endpoint(coap_context, &serv_addr, COAP_PROTO_UDP);
    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create CoAP endpoint");
        coap_free_context(coap_context);
        coap_context = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    // Register /temperature resource (GET)
    temperature_resource = coap_resource_init(coap_make_str_const("temperature"), 0);
    coap_register_handler(temperature_resource, COAP_REQUEST_GET, coap_temperature_handler);
    coap_add_attr(temperature_resource, coap_make_str_const("ct"), coap_make_str_const("50"), 0);
    coap_add_attr(temperature_resource, coap_make_str_const("title"), coap_make_str_const("\"Temperature Sensor\""), 0);
    coap_add_resource(coap_context, temperature_resource);
    
    ESP_LOGI(TAG, "CoAP server started on port %d", COAP_DEFAULT_PORT);
    ESP_LOGI(TAG, "Available resource:");
    ESP_LOGI(TAG, "  GET coap://[ESP32_IP]:5683/temperature");
    
    // Main CoAP server loop
    while (1) {
        int result = coap_io_process(coap_context, 1000);
        if (result < 0) {
            ESP_LOGE(TAG, "CoAP IO process failed");
            break;
        }
    }
    
    // Cleanup
    coap_free_context(coap_context);
    coap_context = NULL;
    vTaskDelete(NULL);
}

esp_err_t coap_server_start(void)
{
    BaseType_t ret = xTaskCreate(coap_server_task, "coap_server", 8192, NULL, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CoAP server task");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

void coap_server_stop(void)
{
    if (coap_context) {
        coap_free_context(coap_context);
        coap_context = NULL;
        ESP_LOGI(TAG, "CoAP server stopped");
    }
}
