idf_component_register(
    SRCS 
        "main.c"
        "app_network.c"
        "bsp/waveshare_rgb_lcd_port.c"
        "ui/ui_app.c"
        "ui/ui_anim.c"
    INCLUDE_DIRS 
        "."
        "bsp"
        "ui"
    REQUIRES 
        esp_lcd 
        driver 
        esp_driver_gpio 
        esp_driver_i2c 
        esp_lvgl_adapter 
        esp_lcd_touch_gt911 
        lvgl
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        mqtt
)



