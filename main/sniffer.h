#pragma once

#include "esp_err.h"
#include "esp_eth_driver.h"

#ifdef __cplusplus
extern "C" {
#endif



esp_err_t sniffer_reg_eth_intf(esp_eth_handle_t eth_handle);

/**
 * @brief Initialize and start packet sniffer for both WiFi and Ethernet
 * 
 * @param channel WiFi channel to monitor (1-13), default is 1 if invalid
 * @param num_packets Number of packets to capture (-1 for unlimited, 0+ for specific count)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t sniffer_init(uint32_t channel, int32_t num_packets);

/**
 * @brief Stop packet sniffer
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t sniffer_stop(void);

#ifdef __cplusplus
}
#endif
