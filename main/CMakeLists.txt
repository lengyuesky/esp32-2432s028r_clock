idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_lcd_ili9341 esp_wifi esp_netif esp_http_client esp_http_server esp_https_ota app_update json mbedtls nvs_flash tcp_transport esp-tls
    EMBED_TXTFILES "github_ota_roots.pem"
)
