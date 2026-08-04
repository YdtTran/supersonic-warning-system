idf_component_register(
    SRCS "ui_dashboard.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl sensor_model coreiot_client esp_wifi esp_app_format spi_flash esp_hw_support esp_timer esp_system
)
