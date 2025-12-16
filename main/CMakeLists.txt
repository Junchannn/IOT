idf_component_register(
    SRCS "cmd_pcap.c" "main.c" "dht11.c" "buzzer.c" "wifi_manager.c" "mqtt_app.c" "sniffer.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_timer esp_wifi esp_netif nvs_flash mqtt console esp_eth
)
