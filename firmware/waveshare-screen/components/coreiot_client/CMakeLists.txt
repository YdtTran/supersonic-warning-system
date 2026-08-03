idf_component_register(
    SRCS "coreiot_client.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_wifi esp_event esp_netif mqtt esp_timer
)
