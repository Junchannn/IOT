#include "buzzer.h"

// For active buzzers - simple on/off
void buzzer_on(void){
    gpio_set_level(BUZZER_GPIO, 1);
    ESP_LOGI(BUZZER_TAG, "Buzzer ON (GPIO23 = HIGH)");
}

void buzzer_off(void){
    gpio_set_level(BUZZER_GPIO, 0);
    ESP_LOGI(BUZZER_TAG, "Buzzer OFF (GPIO23 = LOW)");
}

void buzzer_beep(int freq_hz, int duration_ms){
    ESP_LOGI(BUZZER_TAG, "Beeping at %d Hz for %d ms", freq_hz, duration_ms);
    int period_us      = 1000000 / freq_hz;        
    int half_period_us = period_us / 2;
    int total_time_us  = duration_ms * 1000;
    int cycles         = total_time_us / period_us;

    // Set GPIO to strong drive strength for louder output
    gpio_set_drive_capability(BUZZER_GPIO, GPIO_DRIVE_CAP_3);
    
    for (int i = 0; i < cycles; i++) {
        gpio_set_level(BUZZER_GPIO, 1);
        esp_rom_delay_us(half_period_us);
        gpio_set_level(BUZZER_GPIO, 0);
        esp_rom_delay_us(half_period_us);
    }
    ESP_LOGI(BUZZER_TAG, "Beep complete");
}