#ifndef BUZZER_H
#define BUZZER_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#define BUZZER_TAG "BUZZER"

#define BUZZER_GPIO GPIO_NUM_25  

void buzzer_on(void);
void buzzer_off(void);
void buzzer_beep(int freq_hz, int duration_ms);

#endif