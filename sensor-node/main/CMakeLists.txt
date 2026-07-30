idf_component_register(SRCS "main.c" "app_network.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver
                                esp_driver_ledc
                                esp_driver_gpio
                                esp_driver_gptimer
                                esp_driver_i2c
                                esp_driver_spi
                                esp_driver_uart
                                esp_driver_mcpwm
                                esp_driver_pcnt
                                esp_driver_rmt
                                esp_driver_touch_sens
                                esp_adc
                                esp_lcd
                                esp_timer
                                esp_wifi
                                esp_event
                                esp_netif
                                nvs_flash
                                freertos
                                log
                                vfs
                                fatfs
                                spiffs
                                mqtt
)
