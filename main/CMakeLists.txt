idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_lcd_ili9341 esp_wifi esp_netif esp_http_client esp_http_server json nvs_flash
)
