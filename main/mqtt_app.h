#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "esp_err.h"

esp_err_t mqtt_app_start(void);
void mqtt_publish_temperature(int temperature, int humidity);

#endif
