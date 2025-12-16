#ifndef COAP_APP_H
#define COAP_APP_H

#include "esp_err.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "coap3/coap.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start CoAP server
 * 
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t coap_server_start(void);

/**
 * @brief Update temperature data that will be returned by GET /temperature
 * 
 * @param temperature Temperature in degrees Celsius
 * @param humidity Humidity percentage
 */
void coap_update_temperature(int temperature, int humidity);

/**
 * @brief Stop CoAP server
 */
void coap_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif // COAP_APP_H
