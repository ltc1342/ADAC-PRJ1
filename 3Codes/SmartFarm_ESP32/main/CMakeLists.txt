idf_component_register(
    SRCS
        "main.c"
        "sensor_store.c"
        "uart_handler.c"
        "wifi_manager.c"
        "mqtt_handler.c"
        "web_server.c"
        "power_manager.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_driver_uart # UART 
        esp_driver_gpio # GPIO
        esp_wifi        # Wi-Fi STA
        esp_event       # Default event loop
        nvs_flash       # Non-volatile storage
        esp-mqtt        # esp-mqtt (PubSubClient equivalent)
        esp_http_server # AsyncWebServer equivalent
        esp_netif       # TCP/IP network interface
        freertos        # FreeRTOS tasks, queues, semaphores
        cjson            # cJSON (bundled with ESP-IDF)
        esp_timer       # High-resolution timers
        esp_system      # esp_reset_reason, chip info
)
