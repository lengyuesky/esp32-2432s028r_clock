#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

#define LCD_H_RES 320
#define LCD_V_RES 240
#define LCD_DRAW_LINES 16
#define LCD_SPI_CLOCK_HZ (40 * 1000 * 1000)
#define CLOCK_PANEL_X 12
#define CLOCK_PANEL_Y 20
#define CLOCK_PANEL_W 296
#define CLOCK_PANEL_H 200
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_PROV_DONE_BIT BIT2
#define WIFI_MAX_RETRY 8
#define WIFI_SSID_LEN 33
#define WIFI_PASSWORD_LEN 65
#define PROV_AP_IP_ADDR "192.168.4.1"
#define PROV_HTTP_PORT 80
#define PROV_DNS_PORT 53
#define PROV_DNS_TASK_STACK_SIZE 4096
#define PROV_FORM_MAX_LEN 512
#define WEATHER_HTTP_BUFFER_SIZE 4096
#define WEATHER_GEO_BUFFER_SIZE 768
#define WEATHER_URL_BUFFER_SIZE 320
#define WEATHER_QUERY_CITY_LEN 64
#define WEATHER_IP_LEN 48
#define WEATHER_TEXT_LEN 32
#define WEATHER_CITY_LEN 32
#define WEATHER_FORECAST_DAYS 7
#define WEATHER_TASK_STACK_SIZE 16384
#define CLOCK_SNTP_SYNC_TIMEOUT_MS 30000
#define TOUCH_SAMPLE_COUNT 5
#define TOUCH_DEBOUNCE_TICKS pdMS_TO_TICKS(220)
#define NAV_CARD_Y (CLOCK_PANEL_Y + 156)
#define NAV_CARD_H 38
#define NAV_CARD_W 84
#define NAV_CARD_GAP 8
#define NAV_CARD_X0 (CLOCK_PANEL_X + 14)
#define NAV_CARD_X1 (NAV_CARD_X0 + NAV_CARD_W + NAV_CARD_GAP)
#define NAV_CARD_X2 (NAV_CARD_X1 + NAV_CARD_W + NAV_CARD_GAP)

#ifndef CONFIG_CLOCK_PROV_ENABLE
#define CONFIG_CLOCK_PROV_ENABLE 0
#endif

#ifndef CONFIG_CLOCK_WEATHER_ENABLE
#define CONFIG_CLOCK_WEATHER_ENABLE 0
#endif

#ifndef CONFIG_CLOCK_WEATHER_REFRESH_MINUTES
#define CONFIG_CLOCK_WEATHER_REFRESH_MINUTES 10
#endif

#ifndef CONFIG_CLOCK_WEATHER_CN_GEO_URL
#define CONFIG_CLOCK_WEATHER_CN_GEO_URL ""
#endif

#ifndef CONFIG_CLOCK_TOUCH_ENABLE
#define CONFIG_CLOCK_TOUCH_ENABLE 0
#endif

#ifndef CONFIG_CLOCK_TOUCH_PIN_CS
#define CONFIG_CLOCK_TOUCH_PIN_CS 33
#endif

#ifndef CONFIG_CLOCK_TOUCH_PIN_MOSI
#define CONFIG_CLOCK_TOUCH_PIN_MOSI 32
#endif

#ifndef CONFIG_CLOCK_TOUCH_PIN_MISO
#define CONFIG_CLOCK_TOUCH_PIN_MISO 39
#endif

#ifndef CONFIG_CLOCK_TOUCH_PIN_SCLK
#define CONFIG_CLOCK_TOUCH_PIN_SCLK 25
#endif

#ifndef CONFIG_CLOCK_TOUCH_PIN_IRQ
#define CONFIG_CLOCK_TOUCH_PIN_IRQ 36
#endif

#ifndef CONFIG_CLOCK_TOUCH_IRQ_ACTIVE_LOW
#define CONFIG_CLOCK_TOUCH_IRQ_ACTIVE_LOW 1
#endif

#ifndef CONFIG_CLOCK_TOUCH_SPI_CLOCK_HZ
#define CONFIG_CLOCK_TOUCH_SPI_CLOCK_HZ 2500000
#endif

#ifndef CONFIG_CLOCK_TOUCH_PRESSURE_MIN
#define CONFIG_CLOCK_TOUCH_PRESSURE_MIN 50
#endif

#ifndef CONFIG_CLOCK_TOUCH_RAW_X_MIN
#define CONFIG_CLOCK_TOUCH_RAW_X_MIN 300
#endif

#ifndef CONFIG_CLOCK_TOUCH_RAW_X_MAX
#define CONFIG_CLOCK_TOUCH_RAW_X_MAX 3800
#endif

#ifndef CONFIG_CLOCK_TOUCH_RAW_Y_MIN
#define CONFIG_CLOCK_TOUCH_RAW_Y_MIN 300
#endif

#ifndef CONFIG_CLOCK_TOUCH_RAW_Y_MAX
#define CONFIG_CLOCK_TOUCH_RAW_Y_MAX 3800
#endif

#ifndef CONFIG_CLOCK_TOUCH_SWAP_XY
#define CONFIG_CLOCK_TOUCH_SWAP_XY 0
#endif

#ifndef CONFIG_CLOCK_TOUCH_MIRROR_X
#define CONFIG_CLOCK_TOUCH_MIRROR_X 0
#endif

#ifndef CONFIG_CLOCK_TOUCH_MIRROR_Y
#define CONFIG_CLOCK_TOUCH_MIRROR_Y 0
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef CONFIG_CLOCK_LCD_MIRROR_X
#define CONFIG_CLOCK_LCD_MIRROR_X 0
#endif

#ifndef CONFIG_CLOCK_LCD_MIRROR_Y
#define CONFIG_CLOCK_LCD_MIRROR_Y 0
#endif

static const char *TAG = "cyd_clock";

typedef enum {
    APP_VIEW_DASHBOARD = 0,
    APP_VIEW_ANALOG_CLOCK,
    APP_VIEW_CALENDAR,
    APP_VIEW_WEATHER,
} app_view_t;

typedef struct {
    esp_lcd_panel_handle_t panel;
    SemaphoreHandle_t done_sem;
    uint16_t *draw_buf;
    uint16_t *canvas_buf;
    size_t draw_buf_pixels;
    size_t canvas_buf_pixels;
    int canvas_x;
    int canvas_y;
    int canvas_w;
    int canvas_h;
    bool canvas_active;
} lcd_context_t;

typedef struct {
    bool valid;
    int weather_code;
    int temp_c;
    int humidity;
    time_t updated_at;
    char city[WEATHER_CITY_LEN];
    char condition[WEATHER_TEXT_LEN];
    struct {
        bool valid;
        int min_c;
        int max_c;
        int weather_code;
        char date[11];
        char condition[WEATHER_TEXT_LEN];
    } forecast[WEATHER_FORECAST_DAYS];
    int forecast_count;
} weather_info_t;

typedef struct {
    char *data;
    int len;
    int cap;
} weather_http_buffer_t;

typedef struct {
    bool valid;
    char country_code[4];
    char query_ip[WEATHER_IP_LEN];
    char city_query[WEATHER_QUERY_CITY_LEN];
    char city_display[WEATHER_CITY_LEN];
    char city_native[WEATHER_QUERY_CITY_LEN];
    char region_display[WEATHER_CITY_LEN];
} weather_geo_info_t;

typedef struct {
    char ssid[WIFI_SSID_LEN];
    char password[WIFI_PASSWORD_LEN];
    bool from_nvs;
} wifi_credentials_t;

typedef struct {
    spi_device_handle_t spi;
    bool initialized;
    bool was_pressed;
    TickType_t last_trigger_tick;
} touch_context_t;

static lcd_context_t s_lcd;
static touch_context_t s_touch;
static EventGroupHandle_t s_wifi_event_group;
static SemaphoreHandle_t s_weather_lock;
static weather_info_t s_weather;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_prov_httpd;
static TaskHandle_t s_prov_dns_task;
static char s_prov_ap_ssid[WIFI_SSID_LEN];
static char s_connected_ssid[WIFI_SSID_LEN];
static int s_wifi_retry_count;
static bool s_wifi_started;
static bool s_weather_task_started;
static volatile bool s_wifi_configured;
static volatile bool s_wifi_connected;
static volatile bool s_sta_retry_enabled;
static volatile bool s_prov_active;
static volatile bool s_prov_dns_running;
static volatile bool s_time_synced;
static app_view_t s_current_view = APP_VIEW_DASHBOARD;

static const char s_captive_portal_uri[] = "http://" PROV_AP_IP_ADDR "/";

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t color = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (uint16_t)(b >> 3);
#if CONFIG_CLOCK_LCD_SWAP_COLOR_BYTES
    color = (uint16_t)((color << 8) | (color >> 8));
#endif
    return color;
}

static uint16_t mix_color(uint16_t a, uint16_t b, int num, int den)
{
#if CONFIG_CLOCK_LCD_SWAP_COLOR_BYTES
    a = (uint16_t)((a << 8) | (a >> 8));
    b = (uint16_t)((b << 8) | (b >> 8));
#endif
    int ar = (a >> 11) & 0x1F;
    int ag = (a >> 5) & 0x3F;
    int ab = a & 0x1F;
    int br = (b >> 11) & 0x1F;
    int bg = (b >> 5) & 0x3F;
    int bb = b & 0x1F;
    int rr = ar + ((br - ar) * num) / den;
    int rg = ag + ((bg - ag) * num) / den;
    int rb = ab + ((bb - ab) * num) / den;
    uint16_t color = (uint16_t)((rr << 11) | (rg << 5) | rb);
#if CONFIG_CLOCK_LCD_SWAP_COLOR_BYTES
    color = (uint16_t)((color << 8) | (color >> 8));
#endif
    return color;
}

static bool lcd_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_ctx, &task_woken);
    return task_woken == pdTRUE;
}

static esp_err_t lcd_wait_flush_done(void)
{
    if (xSemaphoreTake(s_lcd.done_sem, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGE(TAG, "LCD flush timeout");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static esp_err_t lcd_draw_bitmap_checked(int x, int y, int w, int h, const uint16_t *pixels)
{
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || pixels == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((x + w) > LCD_H_RES || (y + h) > LCD_V_RES) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)xSemaphoreTake(s_lcd.done_sem, 0);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_lcd.panel, x, y, x + w, y + h, pixels), TAG,
                        "draw bitmap failed");
    return lcd_wait_flush_done();
}

static bool lcd_canvas_is_active(void)
{
    return s_lcd.canvas_active && s_lcd.canvas_buf != NULL;
}

static esp_err_t lcd_fill_rect_canvas(int x, int y, int w, int h, uint16_t color)
{
    if (x >= LCD_H_RES || y >= LCD_V_RES || w <= 0 || h <= 0 || !lcd_canvas_is_active()) {
        return ESP_OK;
    }

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if ((x + w) > LCD_H_RES) {
        w = LCD_H_RES - x;
    }
    if ((y + h) > LCD_V_RES) {
        h = LCD_V_RES - y;
    }
    if (w <= 0 || h <= 0) {
        return ESP_OK;
    }

    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    if (x0 < s_lcd.canvas_x) {
        x0 = s_lcd.canvas_x;
    }
    if (y0 < s_lcd.canvas_y) {
        y0 = s_lcd.canvas_y;
    }
    if (x1 > s_lcd.canvas_x + s_lcd.canvas_w) {
        x1 = s_lcd.canvas_x + s_lcd.canvas_w;
    }
    if (y1 > s_lcd.canvas_y + s_lcd.canvas_h) {
        y1 = s_lcd.canvas_y + s_lcd.canvas_h;
    }
    if (x0 >= x1 || y0 >= y1) {
        return ESP_OK;
    }

    const int local_x = x0 - s_lcd.canvas_x;
    const int local_y = y0 - s_lcd.canvas_y;
    const int fill_w = x1 - x0;
    const int fill_h = y1 - y0;

    for (int row = 0; row < fill_h; ++row) {
        uint16_t *dst = s_lcd.canvas_buf + (size_t)(local_y + row) * (size_t)s_lcd.canvas_w + (size_t)local_x;
        for (int col = 0; col < fill_w; ++col) {
            dst[col] = color;
        }
    }

    return ESP_OK;
}

static esp_err_t lcd_fill_rect_direct(int x, int y, int w, int h, uint16_t color)
{
    if (x >= LCD_H_RES || y >= LCD_V_RES || w <= 0 || h <= 0) {
        return ESP_OK;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if ((x + w) > LCD_H_RES) {
        w = LCD_H_RES - x;
    }
    if ((y + h) > LCD_V_RES) {
        h = LCD_V_RES - y;
    }
    if (w <= 0 || h <= 0) {
        return ESP_OK;
    }

    const int max_lines = (int)(s_lcd.draw_buf_pixels / (size_t)w);
    int lines_per_flush = max_lines > 0 ? max_lines : 1;
    if (lines_per_flush > LCD_DRAW_LINES) {
        lines_per_flush = LCD_DRAW_LINES;
    }

    for (int i = 0; i < w * lines_per_flush; ++i) {
        s_lcd.draw_buf[i] = color;
    }

    for (int row = 0; row < h; row += lines_per_flush) {
        int lines = h - row;
        if (lines > lines_per_flush) {
            lines = lines_per_flush;
        }
        ESP_RETURN_ON_ERROR(lcd_draw_bitmap_checked(x, y + row, w, lines, s_lcd.draw_buf), TAG,
                            "fill rect failed");
    }
    return ESP_OK;
}

static esp_err_t lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (lcd_canvas_is_active()) {
        return lcd_fill_rect_canvas(x, y, w, h, color);
    }
    return lcd_fill_rect_direct(x, y, w, h, color);
}

static esp_err_t lcd_fill_gradient_rect(int x, int y, int w, int h, uint16_t top, uint16_t bottom)
{
    if (x >= LCD_H_RES || y >= LCD_V_RES || w <= 0 || h <= 0) {
        return ESP_OK;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if ((x + w) > LCD_H_RES) {
        w = LCD_H_RES - x;
    }
    if ((y + h) > LCD_V_RES) {
        h = LCD_V_RES - y;
    }
    if (w <= 0 || h <= 0) {
        return ESP_OK;
    }

    if (lcd_canvas_is_active()) {
        for (int row = 0; row < h; ++row) {
            uint16_t color = mix_color(top, bottom, y + row, LCD_V_RES - 1);
            ESP_RETURN_ON_ERROR(lcd_fill_rect(x, y + row, w, 1, color), TAG, "fill canvas gradient failed");
        }
        return ESP_OK;
    }

    for (int row = 0; row < h; ++row) {
        uint16_t color = mix_color(top, bottom, y + row, LCD_V_RES - 1);
        for (int col = 0; col < w; ++col) {
            s_lcd.draw_buf[col] = color;
        }
        ESP_RETURN_ON_ERROR(lcd_draw_bitmap_checked(x, y + row, w, 1, s_lcd.draw_buf), TAG,
                            "fill gradient failed");
    }
    return ESP_OK;
}

static esp_err_t lcd_fill_vertical_blend_rect(int x, int y, int w, int h, uint16_t top, uint16_t bottom)
{
    if (w <= 0 || h <= 0) {
        return ESP_OK;
    }

    int den = h > 1 ? h - 1 : 1;
    for (int row = 0; row < h; ++row) {
        uint16_t color = mix_color(top, bottom, row, den);
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x, y + row, w, 1, color), TAG, "fill blend rect failed");
    }
    return ESP_OK;
}

static esp_err_t lcd_canvas_begin(int x, int y, int w, int h)
{
    ESP_RETURN_ON_FALSE(s_lcd.canvas_buf != NULL, ESP_ERR_INVALID_STATE, TAG, "LCD canvas buffer is unavailable");
    ESP_RETURN_ON_FALSE(w > 0 && h > 0 && (size_t)w * (size_t)h <= s_lcd.canvas_buf_pixels,
                        ESP_ERR_INVALID_ARG, TAG, "invalid LCD canvas size");

    s_lcd.canvas_x = x;
    s_lcd.canvas_y = y;
    s_lcd.canvas_w = w;
    s_lcd.canvas_h = h;
    s_lcd.canvas_active = true;
    return ESP_OK;
}

static esp_err_t lcd_canvas_fill_gradient(uint16_t top, uint16_t bottom)
{
    ESP_RETURN_ON_FALSE(lcd_canvas_is_active(), ESP_ERR_INVALID_STATE, TAG, "LCD canvas is not active");

    for (int row = 0; row < s_lcd.canvas_h; ++row) {
        uint16_t color = mix_color(top, bottom, s_lcd.canvas_y + row, LCD_V_RES - 1);
        uint16_t *dst = s_lcd.canvas_buf + (size_t)row * (size_t)s_lcd.canvas_w;
        for (int col = 0; col < s_lcd.canvas_w; ++col) {
            dst[col] = color;
        }
    }
    return ESP_OK;
}

static esp_err_t lcd_canvas_flush(void)
{
    ESP_RETURN_ON_FALSE(lcd_canvas_is_active(), ESP_ERR_INVALID_STATE, TAG, "LCD canvas is not active");

    const int x = s_lcd.canvas_x;
    const int y = s_lcd.canvas_y;
    const int w = s_lcd.canvas_w;
    const int h = s_lcd.canvas_h;
    s_lcd.canvas_active = false;

    int lines_per_flush = (int)(s_lcd.draw_buf_pixels / (size_t)w);
    if (lines_per_flush <= 0) {
        lines_per_flush = 1;
    }
    if (lines_per_flush > LCD_DRAW_LINES) {
        lines_per_flush = LCD_DRAW_LINES;
    }

    for (int row = 0; row < h; row += lines_per_flush) {
        int lines = h - row;
        if (lines > lines_per_flush) {
            lines = lines_per_flush;
        }
        memcpy(s_lcd.draw_buf, s_lcd.canvas_buf + (size_t)row * (size_t)w,
               (size_t)w * (size_t)lines * sizeof(uint16_t));
        ESP_RETURN_ON_ERROR(lcd_draw_bitmap_checked(x, y + row, w, lines, s_lcd.draw_buf),
                            TAG, "flush canvas failed");
    }
    return ESP_OK;
}

static esp_err_t lcd_draw_hline(int x, int y, int w, uint16_t color)
{
    return lcd_fill_rect(x, y, w, 1, color);
}

static esp_err_t lcd_draw_rect_outline(int x, int y, int w, int h, uint16_t color)
{
    ESP_RETURN_ON_ERROR(lcd_fill_rect(x, y, w, 1, color), TAG, "draw rect top failed");
    ESP_RETURN_ON_ERROR(lcd_fill_rect(x, y + h - 1, w, 1, color), TAG, "draw rect bottom failed");
    ESP_RETURN_ON_ERROR(lcd_fill_rect(x, y, 1, h, color), TAG, "draw rect left failed");
    return lcd_fill_rect(x + w - 1, y, 1, h, color);
}

static esp_err_t lcd_fill_circle(int cx, int cy, int radius, uint16_t color)
{
    for (int y = -radius; y <= radius; ++y) {
        int x_span = (int)sqrtf((float)(radius * radius - y * y));
        ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - x_span, cy + y, x_span * 2 + 1, 1, color), TAG,
                            "fill circle failed");
    }
    return ESP_OK;
}

static esp_err_t lcd_draw_line(int x0, int y0, int x1, int y1, int thickness, uint16_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int r = thickness > 1 ? thickness / 2 : 0;

    for (;;) {
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x0 - r, y0 - r, thickness, thickness, color),
                            TAG, "draw line failed");
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
    return ESP_OK;
}

static void sort_triangle_points_by_y(int *x0, int *y0, int *x1, int *y1, int *x2, int *y2)
{
    if (*y0 > *y1) {
        int tx = *x0;
        int ty = *y0;
        *x0 = *x1;
        *y0 = *y1;
        *x1 = tx;
        *y1 = ty;
    }
    if (*y1 > *y2) {
        int tx = *x1;
        int ty = *y1;
        *x1 = *x2;
        *y1 = *y2;
        *x2 = tx;
        *y2 = ty;
    }
    if (*y0 > *y1) {
        int tx = *x0;
        int ty = *y0;
        *x0 = *x1;
        *y0 = *y1;
        *x1 = tx;
        *y1 = ty;
    }
}

static esp_err_t lcd_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color)
{
    sort_triangle_points_by_y(&x0, &y0, &x1, &y1, &x2, &y2);

    if (y0 == y2) {
        int min_x = x0;
        int max_x = x0;
        if (x1 < min_x) {
            min_x = x1;
        }
        if (x2 < min_x) {
            min_x = x2;
        }
        if (x1 > max_x) {
            max_x = x1;
        }
        if (x2 > max_x) {
            max_x = x2;
        }
        return lcd_fill_rect(min_x, y0, max_x - min_x + 1, 1, color);
    }

    for (int y = y0; y <= y2; ++y) {
        int xa = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        int xb = 0;
        if (y < y1) {
            if (y1 == y0) {
                continue;
            }
            xb = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        } else {
            if (y2 == y1) {
                xb = x1;
            } else {
                xb = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            }
        }
        if (xa > xb) {
            int tmp = xa;
            xa = xb;
            xb = tmp;
        }
        ESP_RETURN_ON_ERROR(lcd_fill_rect(xa, y, xb - xa + 1, 1, color), TAG, "fill triangle failed");
    }
    return ESP_OK;
}

static esp_err_t lcd_fill_round_rect(int x, int y, int w, int h, int radius, uint16_t color)
{
    ESP_RETURN_ON_ERROR(lcd_fill_rect(x + radius, y, w - 2 * radius, h, color), TAG,
                        "round rect body failed");
    ESP_RETURN_ON_ERROR(lcd_fill_rect(x, y + radius, radius, h - 2 * radius, color), TAG,
                        "round rect left failed");
    ESP_RETURN_ON_ERROR(lcd_fill_rect(x + w - radius, y + radius, radius, h - 2 * radius, color), TAG,
                        "round rect right failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(x + radius, y + radius, radius, color), TAG,
                        "round rect corner failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(x + w - radius - 1, y + radius, radius, color), TAG,
                        "round rect corner failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(x + radius, y + h - radius - 1, radius, color), TAG,
                        "round rect corner failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(x + w - radius - 1, y + h - radius - 1, radius, color), TAG,
                        "round rect corner failed");
    return ESP_OK;
}

static const uint8_t font5x7[][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
};

typedef struct {
    const char *utf8;
    uint16_t rows[16];
} zh16_glyph_t;

static const zh16_glyph_t zh16_glyphs[] = {
    {"时", {0x0008, 0x0008, 0x7C08, 0x65FE, 0x65FE, 0x6408, 0x7C88, 0x64C8, 0x6448, 0x6408, 0x7C08, 0x6408, 0x6008, 0x0038, 0x0000, 0x0000}},
    {"间", {0x1000, 0x19FC, 0x0804, 0x6004, 0x67E4, 0x6424, 0x6424, 0x67E4, 0x6424, 0x6424, 0x67E4, 0x6004, 0x6004, 0x600C, 0x0000, 0x0000}},
    {"日", {0x1FF8, 0x1FF8, 0x1808, 0x1808, 0x1808, 0x1808, 0x1FF8, 0x1808, 0x1808, 0x1808, 0x1808, 0x1FF8, 0x1FF8, 0x1808, 0x0000, 0x0000}},
    {"期", {0x3300, 0x333E, 0x7FA2, 0x3322, 0x3322, 0x3F3E, 0x3322, 0x3F22, 0x333E, 0x7FE2, 0x0062, 0x3242, 0x61C2, 0x4186, 0x0080, 0x0000}},
    {"湿", {0x0000, 0x23FC, 0x1A04, 0x03FC, 0x0204, 0x6204, 0x33FC, 0x0000, 0x0492, 0x1492, 0x2294, 0x229C, 0x6090, 0x4FFE, 0x0000, 0x0000}},
    {"度", {0x0080, 0x0080, 0x3FFC, 0x2210, 0x2210, 0x2FFC, 0x2210, 0x23F0, 0x2000, 0x2FF8, 0x2310, 0x21A0, 0x60C0, 0x473E, 0x0802, 0x0000}},
    {"星", {0x1FF8, 0x1008, 0x1FF8, 0x1008, 0x1FF8, 0x0000, 0x1980, 0x3FFC, 0x2180, 0x4180, 0x1FF8, 0x0180, 0x7FFE, 0x7FFE, 0x0000, 0x0000}},
    {"一", {0x0000, 0x0000, 0x0000, 0x0000, 0x7FFE, 0x7FFE, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}},
    {"二", {0x0000, 0x0000, 0x0000, 0x3FF8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7FFE, 0x7FFE, 0x0000, 0x0000, 0x0000}},
    {"三", {0x0000, 0x0000, 0x3FFC, 0x0000, 0x0000, 0x0000, 0x1FF8, 0x1FF8, 0x0000, 0x0000, 0x0000, 0x0000, 0x7FFE, 0x0000, 0x0000, 0x0000}},
    {"四", {0x0000, 0x3FFC, 0x2244, 0x2244, 0x2244, 0x2244, 0x2644, 0x2444, 0x2C74, 0x2804, 0x2004, 0x2004, 0x3FFC, 0x2004, 0x0000, 0x0000}},
    {"五", {0x0000, 0x3FFC, 0x3FFC, 0x0100, 0x0300, 0x0300, 0x1FF0, 0x0210, 0x0210, 0x0210, 0x0210, 0x0610, 0x7FFE, 0x4000, 0x0000, 0x0000}},
    {"六", {0x0000, 0x0300, 0x0180, 0x0080, 0x7FFE, 0x7FFE, 0x0000, 0x0420, 0x0630, 0x0C30, 0x0818, 0x180C, 0x3004, 0x2004, 0x0000, 0x0000}},
    {"天", {0x3FF8, 0x0180, 0x0180, 0x0180, 0x7FFE, 0x7FFE, 0x0180, 0x03C0, 0x0240, 0x0620, 0x0C30, 0x380C, 0x6006, 0x0000, 0x0000, 0x0000}},
    {"配", {0x7F7C, 0x7F7C, 0x1404, 0x7F04, 0x5504, 0x5504, 0x557C, 0x5540, 0x6340, 0x4140, 0x7F40, 0x4140, 0x7F42, 0x417E, 0x0000, 0x0000}},
    {"网", {0x7FFE, 0x6006, 0x611E, 0x6B16, 0x6AD6, 0x6676, 0x6626, 0x6676, 0x6F56, 0x68DE, 0x7186, 0x6006, 0x600E, 0x6000, 0x0000, 0x0000}},
    {"未", {0x0100, 0x0100, 0x0100, 0x3FF8, 0x0100, 0x0100, 0x7FFE, 0x0380, 0x0740, 0x0D60, 0x1930, 0x311C, 0x6106, 0x0100, 0x0000, 0x0000}},
    {"连", {0x0040, 0x6080, 0x37FE, 0x1100, 0x0120, 0x0220, 0x73FC, 0x1020, 0x1020, 0x17FE, 0x1020, 0x1020, 0x3820, 0x47FE, 0x0000, 0x0000}},
    {"接", {0x1020, 0x1020, 0x13FE, 0x7C88, 0x1088, 0x1010, 0x17FE, 0x1C40, 0x73FE, 0x1088, 0x1198, 0x10F0, 0x1078, 0x73C6, 0x0200, 0x0000}},
    {"北", {0x0640, 0x0640, 0x0640, 0x0640, 0x0646, 0x7E58, 0x0660, 0x0640, 0x0640, 0x0640, 0x0640, 0x3E42, 0x6642, 0x067E, 0x0000, 0x0000}},
    {"京", {0x0100, 0x0180, 0x7FFE, 0x0000, 0x1FF8, 0x1818, 0x1818, 0x1818, 0x1FF8, 0x0080, 0x0CB0, 0x1898, 0x3086, 0x0380, 0x0200, 0x0000}},
    {"启", {0x0180, 0x0080, 0x1FFC, 0x100C, 0x100C, 0x1FFC, 0x1FFC, 0x1000, 0x17FC, 0x140C, 0x340C, 0x240C, 0x67FC, 0x440C, 0x040C, 0x0000}},
    {"动", {0x0030, 0x0030, 0x3E30, 0x0030, 0x00FE, 0x7F32, 0x7F22, 0x1022, 0x1622, 0x2222, 0x2362, 0x7D46, 0x00C6, 0x019C, 0x0000, 0x0000}},
    {"中", {0x0180, 0x0180, 0x0180, 0x3FFC, 0x2184, 0x2184, 0x2184, 0x2184, 0x3FFC, 0x2184, 0x0180, 0x0180, 0x0180, 0x0180, 0x0180, 0x0000}},
    {"对", {0x0008, 0x0008, 0x7E08, 0x0208, 0x03FE, 0x2408, 0x3488, 0x1C88, 0x0C48, 0x0C48, 0x1608, 0x3208, 0x6008, 0x4038, 0x0000, 0x0000}},
    {"晴", {0x0020, 0x03FE, 0x7820, 0x49FC, 0x4820, 0x4BFE, 0x7800, 0x49FC, 0x4904, 0x49FC, 0x7904, 0x49FC, 0x4104, 0x010C, 0x0108, 0x0000}},
    {"雨", {0x7FFE, 0x0080, 0x0080, 0x3FFC, 0x2084, 0x2CA4, 0x2694, 0x2084, 0x20A4, 0x24B4, 0x2084, 0x2084, 0x208C, 0x0000, 0x0000, 0x0000}},
    {"雪", {0x0000, 0x1FF8, 0x0080, 0x7FFE, 0x6082, 0x6EFA, 0x1EB8, 0x0000, 0x2000, 0x3FFC, 0x000C, 0x1FFC, 0x000C, 0x1FFC, 0x000C, 0x0000}},
    {"雾", {0x3FFC, 0x0080, 0x7FFE, 0x6082, 0x6CB2, 0x1EF8, 0x0400, 0x1FF0, 0x3360, 0x07E0, 0x791E, 0x1FF8, 0x0218, 0x3C70, 0x0000, 0x0000}},
    {"多", {0x0200, 0x0600, 0x0FF8, 0x1030, 0x2260, 0x0180, 0x0760, 0x38FE, 0x0304, 0x060C, 0x1898, 0x0070, 0x00C0, 0x3F00, 0x3000, 0x0000}},
    {"云", {0x0000, 0x1FF8, 0x0000, 0x0000, 0x0000, 0x7FFE, 0x7FFE, 0x0300, 0x0200, 0x0420, 0x0C30, 0x1818, 0x3FFC, 0x1804, 0x0000, 0x0000}},
    {"雷", {0x3FFC, 0x0180, 0x7FFE, 0x4182, 0x5DBA, 0x0180, 0x1DB8, 0x0000, 0x1FF8, 0x1188, 0x1FF8, 0x1188, 0x1188, 0x1FF8, 0x1008, 0x0000}},
    {"气", {0x0C00, 0x0800, 0x1FFC, 0x3000, 0x2FF8, 0x6000, 0x0000, 0x3FF0, 0x0010, 0x0010, 0x0010, 0x001A, 0x000A, 0x000E, 0x0000, 0x0000}},
};

static const uint8_t *font_for_char(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return font5x7[ch - '0'];
    }
    ch = (char)toupper((unsigned char)ch);
    if (ch >= 'A' && ch <= 'Z') {
        return font5x7[10 + ch - 'A'];
    }
    return NULL;
}

static esp_err_t draw_char5(int x, int y, char ch, int scale, uint16_t color)
{
    if (ch == ' ') {
        return ESP_OK;
    }
    if (ch == ':') {
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + 2 * scale, y + 2 * scale, scale, scale, color), TAG,
                            "draw colon failed");
        return lcd_fill_rect(x + 2 * scale, y + 5 * scale, scale, scale, color);
    }
    if (ch == '-' || ch == '/') {
        return lcd_fill_rect(x + scale, y + 3 * scale, 3 * scale, scale, color);
    }
    if (ch == '+') {
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + scale, y + 3 * scale, 3 * scale, scale, color), TAG,
                            "draw plus failed");
        return lcd_fill_rect(x + 2 * scale, y + 2 * scale, scale, 3 * scale, color);
    }
    if (ch == '%') {
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x, y + scale, scale, scale, color), TAG,
                            "draw percent failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + 4 * scale, y + 5 * scale, scale, scale, color), TAG,
                            "draw percent failed");
        for (int i = 0; i < 5; ++i) {
            ESP_RETURN_ON_ERROR(lcd_fill_rect(x + (4 - i) * scale, y + (1 + i) * scale,
                                              scale, scale, color), TAG, "draw percent failed");
        }
        return ESP_OK;
    }
    if (ch == '.') {
        return lcd_fill_rect(x + 2 * scale, y + 6 * scale, scale, scale, color);
    }

    const uint8_t *glyph = font_for_char(ch);
    if (glyph == NULL) {
        return ESP_OK;
    }

    for (int col = 0; col < 5; ++col) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; ++row) {
            if ((bits >> row) & 0x01) {
                ESP_RETURN_ON_ERROR(lcd_fill_rect(x + col * scale, y + row * scale, scale, scale, color),
                                    TAG, "draw glyph failed");
            }
        }
    }
    return ESP_OK;
}

static esp_err_t draw_text5(int x, int y, const char *text, int scale, uint16_t color)
{
    int cursor = x;
    while (*text != '\0') {
        ESP_RETURN_ON_ERROR(draw_char5(cursor, y, *text, scale, color), TAG, "draw text failed");
        cursor += 6 * scale;
        ++text;
    }
    return ESP_OK;
}

static int text5_width(const char *text, int scale)
{
    return (int)strlen(text) * 6 * scale;
}

static int utf8_char_len(unsigned char ch)
{
    if ((ch & 0x80) == 0) {
        return 1;
    }
    if ((ch & 0xE0) == 0xC0) {
        return 2;
    }
    if ((ch & 0xF0) == 0xE0) {
        return 3;
    }
    if ((ch & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

static const zh16_glyph_t *find_zh16_glyph(const char *text, int *utf8_len)
{
    int len = utf8_char_len((unsigned char)*text);
    if (utf8_len != NULL) {
        *utf8_len = len;
    }
    for (size_t i = 0; i < sizeof(zh16_glyphs) / sizeof(zh16_glyphs[0]); ++i) {
        if (strncmp(text, zh16_glyphs[i].utf8, (size_t)len) == 0 && zh16_glyphs[i].utf8[len] == '\0') {
            return &zh16_glyphs[i];
        }
    }
    return NULL;
}

static esp_err_t draw_zh16_glyph(int x, int y, const zh16_glyph_t *glyph, int scale, uint16_t color)
{
    if (glyph == NULL) {
        return lcd_draw_rect_outline(x, y, 8 * scale, 12 * scale, color);
    }

    for (int row = 0; row < 16; ++row) {
        uint16_t bits = glyph->rows[row];
        for (int col = 0; col < 16; ++col) {
            if ((bits >> (15 - col)) & 0x01) {
                ESP_RETURN_ON_ERROR(lcd_fill_rect(x + col * scale, y + row * scale,
                                                  scale, scale, color), TAG, "draw zh glyph failed");
            }
        }
    }
    return ESP_OK;
}

static int text_mixed_width(const char *text, int scale)
{
    int width = 0;
    while (text != NULL && *text != '\0') {
        unsigned char ch = (unsigned char)*text;
        if ((ch & 0x80) == 0) {
            width += 6 * scale;
            ++text;
        } else {
            int len = utf8_char_len(ch);
            width += 17 * scale;
            text += len;
        }
    }
    return width;
}

static esp_err_t draw_text_mixed_limited(int x, int y, const char *text,
                                         int max_w, int scale, uint16_t color)
{
    int cursor = x;
    while (text != NULL && *text != '\0') {
        unsigned char ch = (unsigned char)*text;
        if ((ch & 0x80) == 0) {
            int char_w = 6 * scale;
            if (cursor + char_w > x + max_w) {
                break;
            }
            ESP_RETURN_ON_ERROR(draw_char5(cursor, y + 4 * scale, *text, scale, color),
                                TAG, "draw mixed ascii failed");
            cursor += char_w;
            ++text;
        } else {
            int len = 0;
            const zh16_glyph_t *glyph = find_zh16_glyph(text, &len);
            int char_w = 17 * scale;
            if (cursor + char_w > x + max_w) {
                break;
            }
            ESP_RETURN_ON_ERROR(draw_zh16_glyph(cursor, y, glyph, scale, color),
                                TAG, "draw mixed zh failed");
            cursor += char_w;
            text += len;
        }
    }
    return ESP_OK;
}

static esp_err_t draw_text_mixed(int x, int y, const char *text, int scale, uint16_t color)
{
    return draw_text_mixed_limited(x, y, text, LCD_H_RES, scale, color);
}

static esp_err_t draw_text_mixed_centered(int center_x, int y, const char *text, int scale, uint16_t color)
{
    return draw_text_mixed(center_x - text_mixed_width(text, scale) / 2, y, text, scale, color);
}

static esp_err_t draw_text_mixed_centered_limited(int center_x, int y, const char *text,
                                                  int max_w, int scale, uint16_t color)
{
    int width = text_mixed_width(text, scale);
    int x = center_x - width / 2;
    if (width > max_w) {
        x = center_x - max_w / 2;
    }
    return draw_text_mixed_limited(x, y, text, max_w, scale, color);
}

static esp_err_t draw_text5_limited(int x, int y, const char *text, size_t max_chars, int scale, uint16_t color)
{
    char clipped[WEATHER_TEXT_LEN] = {0};
    size_t n = strlen(text);
    if (n > max_chars) {
        n = max_chars;
    }
    if (n >= sizeof(clipped)) {
        n = sizeof(clipped) - 1;
    }
    memcpy(clipped, text, n);
    clipped[n] = '\0';
    return draw_text5(x, y, clipped, scale, color);
}

static esp_err_t draw_text5_centered(int center_x, int y, const char *text, int scale, uint16_t color)
{
    return draw_text5(center_x - text5_width(text, scale) / 2, y, text, scale, color);
}

static esp_err_t draw_time_digits(const struct tm *tm)
{
    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm->tm_hour, tm->tm_min);

    const int scale = 7;
    const int x = 160 - text5_width(time_buf, scale) / 2;
    const int y = CLOCK_PANEL_Y + 48;
    ESP_RETURN_ON_ERROR(draw_text5(x + 2, y + 3, time_buf, scale, rgb565(4, 12, 28)),
                        TAG, "draw time shadow failed");
    return draw_text5(x, y, time_buf, scale, rgb565(219, 246, 255));
}

static esp_err_t draw_progress_dots(int second)
{
    const int cx = 160;
    const int y = CLOCK_PANEL_Y + CLOCK_PANEL_H - 5;
    const uint16_t dim = rgb565(42, 50, 80);
    const uint16_t cyan = rgb565(76, 226, 255);
    const uint16_t violet = rgb565(139, 89, 255);
    const uint16_t pink = rgb565(255, 103, 184);
    const uint16_t colors[] = {cyan, violet, pink};
    int active = second / 20;

    for (int i = 0; i < 3; ++i) {
        uint16_t color = i == active ? colors[i] : dim;
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 18 + i * 18, y, 3, color),
                            TAG, "draw progress dot failed");
    }
    return ESP_OK;
}

static esp_err_t draw_panel_frame(void)
{
    const uint16_t outer = rgb565(22, 32, 58);
    const uint16_t inner = rgb565(6, 12, 27);

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(CLOCK_PANEL_X, CLOCK_PANEL_Y,
                                            CLOCK_PANEL_W, CLOCK_PANEL_H, 9, outer),
                        TAG, "draw panel outer failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(CLOCK_PANEL_X + 1, CLOCK_PANEL_Y + 1,
                                            CLOCK_PANEL_W - 2, CLOCK_PANEL_H - 2, 8, inner),
                        TAG, "draw panel inner failed");
    return ESP_OK;
}

static esp_err_t draw_star_field(void)
{
    static const uint16_t stars[][2] = {
        {221, 45}, {246, 55}, {286, 65}, {238, 81}, {266, 89},
        {209, 96}, {281, 105}, {232, 112}, {298, 120}, {255, 128},
    };
    const uint16_t star = rgb565(215, 223, 255);
    const uint16_t star_dim = rgb565(103, 118, 168);

    for (size_t i = 0; i < sizeof(stars) / sizeof(stars[0]); ++i) {
        int r = (i % 4) == 0 ? 2 : 1;
        ESP_RETURN_ON_ERROR(lcd_fill_circle(stars[i][0], stars[i][1], r,
                                            (i % 3) == 0 ? star : star_dim),
                            TAG, "draw star failed");
    }
    return ESP_OK;
}

static esp_err_t draw_scenic_background(void)
{
    const uint16_t sky_top = rgb565(4, 7, 24);
    const uint16_t sky_mid = rgb565(13, 24, 70);
    const uint16_t lake_top = rgb565(20, 31, 84);
    const uint16_t lake_bottom = rgb565(4, 11, 31);
    const uint16_t glow = rgb565(255, 117, 181);
    const uint16_t moon = rgb565(255, 202, 122);

    ESP_RETURN_ON_ERROR(lcd_fill_vertical_blend_rect(CLOCK_PANEL_X + 1, CLOCK_PANEL_Y + 1,
                                                     CLOCK_PANEL_W - 2, 116,
                                                     sky_top, sky_mid),
                        TAG, "draw sky failed");
    ESP_RETURN_ON_ERROR(lcd_fill_vertical_blend_rect(CLOCK_PANEL_X + 1, CLOCK_PANEL_Y + 116,
                                                     CLOCK_PANEL_W - 2, CLOCK_PANEL_H - 117,
                                                     lake_top, lake_bottom),
                        TAG, "draw lake failed");
    ESP_RETURN_ON_ERROR(draw_star_field(), TAG, "draw stars failed");

    ESP_RETURN_ON_ERROR(lcd_fill_circle(249, 118, 17, rgb565(255, 165, 129)), TAG, "draw sunset failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(249, 118, 13, moon), TAG, "draw sun core failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(12, 151, 76, 94, 145, 151, rgb565(5, 16, 43)),
                        TAG, "draw mountain failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(91, 151, 163, 104, 220, 151, rgb565(12, 29, 78)),
                        TAG, "draw mountain failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(174, 151, 251, 109, 310, 151, rgb565(16, 36, 92)),
                        TAG, "draw mountain failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(12, 165, 63, 126, 128, 165, rgb565(2, 10, 28)),
                        TAG, "draw foreground mountain failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(185, 165, 279, 128, 312, 165, rgb565(3, 13, 33)),
                        TAG, "draw foreground mountain failed");

    ESP_RETURN_ON_ERROR(lcd_draw_hline(87, 153, 164, rgb565(76, 98, 170)), TAG, "draw horizon failed");
    ESP_RETURN_ON_ERROR(lcd_draw_hline(87, 154, 164, rgb565(50, 68, 132)), TAG, "draw horizon failed");
    ESP_RETURN_ON_ERROR(lcd_fill_vertical_blend_rect(130, 155, 92, 8,
                                                     rgb565(251, 126, 198), rgb565(96, 83, 230)),
                        TAG, "draw reflection failed");
    ESP_RETURN_ON_ERROR(lcd_draw_line(118, 166, 231, 166, 1, rgb565(111, 95, 255)),
                        TAG, "draw lake line failed");
    ESP_RETURN_ON_ERROR(lcd_draw_line(154, 160, 214, 160, 1, glow), TAG, "draw lake glow failed");
    return lcd_draw_line(168, 170, 203, 170, 1, rgb565(74, 226, 255));
}

static esp_err_t draw_static_background(void)
{
    const uint16_t top = rgb565(2, 4, 11);
    const uint16_t bottom = rgb565(7, 13, 27);

    ESP_RETURN_ON_ERROR(lcd_fill_gradient_rect(0, 0, LCD_H_RES, LCD_V_RES, top, bottom), TAG,
                        "draw background failed");
    return draw_panel_frame();
}

static esp_err_t weather_state_init(void)
{
    s_weather_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_weather_lock != NULL, ESP_ERR_NO_MEM, TAG, "create weather lock failed");
    s_weather.valid = false;
    s_weather.weather_code = -1;
    s_weather.temp_c = 0;
    s_weather.humidity = 0;
    s_weather.updated_at = 0;
    s_weather.forecast_count = 0;
    s_weather.city[0] = '\0';
    strlcpy(s_weather.condition, "WAIT", sizeof(s_weather.condition));
    return ESP_OK;
}

static void weather_get_snapshot(weather_info_t *out)
{
    if (out == NULL) {
        return;
    }

    if (s_weather_lock != NULL && xSemaphoreTake(s_weather_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
        *out = s_weather;
        xSemaphoreGive(s_weather_lock);
        return;
    }

    *out = s_weather;
}

static void weather_set_snapshot(const weather_info_t *info)
{
    if (info == NULL) {
        return;
    }

    if (s_weather_lock != NULL && xSemaphoreTake(s_weather_lock, pdMS_TO_TICKS(1000)) == pdTRUE) {
        s_weather = *info;
        xSemaphoreGive(s_weather_lock);
    }
}

static char *trim_ascii(char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
        *end = '\0';
    }
    return text;
}

static bool parse_first_int(const char *text, int *value)
{
    if (text == NULL || value == NULL) {
        return false;
    }

    while (*text != '\0' && *text != '-' && *text != '+' && !isdigit((unsigned char)*text)) {
        ++text;
    }
    if (*text == '\0') {
        return false;
    }

    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text) {
        return false;
    }

    *value = (int)parsed;
    return true;
}

static void sanitize_weather_condition(const char *input, char *output, size_t output_len)
{
    if (output_len == 0) {
        return;
    }

    size_t n = 0;
    bool last_space = false;
    for (const unsigned char *p = (const unsigned char *)input; p != NULL && *p != '\0'; ++p) {
        char ch = (char)*p;
        if (isalpha(*p)) {
            ch = (char)toupper(*p);
        } else if (isdigit(*p)) {
            ch = (char)*p;
        } else if (*p == ' ' || *p == '-' || *p == '_') {
            ch = ' ';
        } else {
            continue;
        }

        if (ch == ' ') {
            if (last_space || n == 0) {
                continue;
            }
            last_space = true;
        } else {
            last_space = false;
        }

        if (n + 1 >= output_len) {
            break;
        }
        output[n++] = ch;
    }

    while (n > 0 && output[n - 1] == ' ') {
        --n;
    }
    output[n] = '\0';
    if (n == 0) {
        strlcpy(output, "WEATHER", output_len);
    }
}

static void sanitize_weather_city(const char *input, char *output, size_t output_len)
{
    if (output_len == 0) {
        return;
    }

    size_t n = 0;
    bool last_space = true;
    for (const unsigned char *p = (const unsigned char *)input; p != NULL && *p != '\0'; ++p) {
        if (*p == ',' || *p == '|') {
            break;
        }

        char ch = '\0';
        if (isalnum(*p)) {
            ch = (char)*p;
        } else if (*p == ' ' || *p == '-' || *p == '_') {
            ch = ' ';
        } else {
            continue;
        }

        if (ch == ' ') {
            if (last_space) {
                continue;
            }
            last_space = true;
        } else {
            last_space = false;
        }

        if (n + 1 >= output_len) {
            break;
        }
        output[n++] = ch;
    }

    while (n > 0 && output[n - 1] == ' ') {
        --n;
    }
    output[n] = '\0';
}

typedef struct {
    const char *needle;
    const char *ascii;
} weather_city_alias_t;

static const char *weather_city_ascii_alias(const char *city)
{
    static const weather_city_alias_t aliases[] = {
        {"北京", "Beijing"},       {"上海", "Shanghai"},     {"天津", "Tianjin"},
        {"重庆", "Chongqing"},     {"广州", "Guangzhou"},   {"深圳", "Shenzhen"},
        {"珠海", "Zhuhai"},       {"佛山", "Foshan"},      {"东莞", "Dongguan"},
        {"中山", "Zhongshan"},    {"惠州", "Huizhou"},     {"汕头", "Shantou"},
        {"杭州", "Hangzhou"},     {"宁波", "Ningbo"},      {"温州", "Wenzhou"},
        {"绍兴", "Shaoxing"},     {"嘉兴", "Jiaxing"},     {"金华", "Jinhua"},
        {"台州", "Taizhou"},      {"湖州", "Huzhou"},      {"南京", "Nanjing"},
        {"苏州", "Suzhou"},       {"无锡", "Wuxi"},        {"常州", "Changzhou"},
        {"南通", "Nantong"},      {"扬州", "Yangzhou"},    {"徐州", "Xuzhou"},
        {"盐城", "Yancheng"},     {"泰州", "Taizhou"},     {"济南", "Jinan"},
        {"青岛", "Qingdao"},      {"烟台", "Yantai"},      {"潍坊", "Weifang"},
        {"临沂", "Linyi"},        {"济宁", "Jining"},      {"淄博", "Zibo"},
        {"威海", "Weihai"},       {"泰安", "Taian"},       {"德州", "Dezhou"},
        {"郑州", "Zhengzhou"},    {"洛阳", "Luoyang"},     {"开封", "Kaifeng"},
        {"新乡", "Xinxiang"},     {"南阳", "Nanyang"},     {"武汉", "Wuhan"},
        {"襄阳", "Xiangyang"},    {"宜昌", "Yichang"},     {"长沙", "Changsha"},
        {"株洲", "Zhuzhou"},      {"湘潭", "Xiangtan"},    {"衡阳", "Hengyang"},
        {"成都", "Chengdu"},      {"绵阳", "Mianyang"},    {"德阳", "Deyang"},
        {"乐山", "Leshan"},       {"宜宾", "Yibin"},       {"西安", "Xian"},
        {"咸阳", "Xianyang"},     {"宝鸡", "Baoji"},       {"合肥", "Hefei"},
        {"芜湖", "Wuhu"},         {"蚌埠", "Bengbu"},      {"福州", "Fuzhou"},
        {"厦门", "Xiamen"},       {"泉州", "Quanzhou"},    {"漳州", "Zhangzhou"},
        {"南昌", "Nanchang"},     {"九江", "Jiujiang"},    {"赣州", "Ganzhou"},
        {"石家庄", "Shijiazhuang"}, {"唐山", "Tangshan"},  {"保定", "Baoding"},
        {"廊坊", "Langfang"},     {"秦皇岛", "Qinhuangdao"}, {"太原", "Taiyuan"},
        {"大同", "Datong"},       {"沈阳", "Shenyang"},    {"大连", "Dalian"},
        {"鞍山", "Anshan"},       {"长春", "Changchun"},   {"吉林", "Jilin"},
        {"哈尔滨", "Harbin"},     {"大庆", "Daqing"},      {"呼和浩特", "Hohhot"},
        {"包头", "Baotou"},       {"南宁", "Nanning"},     {"桂林", "Guilin"},
        {"柳州", "Liuzhou"},      {"海口", "Haikou"},      {"三亚", "Sanya"},
        {"贵阳", "Guiyang"},      {"遵义", "Zunyi"},       {"昆明", "Kunming"},
        {"大理", "Dali"},         {"拉萨", "Lhasa"},       {"兰州", "Lanzhou"},
        {"天水", "Tianshui"},     {"西宁", "Xining"},      {"银川", "Yinchuan"},
        {"乌鲁木齐", "Urumqi"},
    };

    if (city == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
        if (strstr(city, aliases[i].needle) != NULL) {
            return aliases[i].ascii;
        }
    }
    return NULL;
}

static bool ascii_equals_ignore_case(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return false;
    }

    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static bool weather_geo_is_excluded_region(const char *country_code, const char *country,
                                           const char *region, const char *city)
{
    if (ascii_equals_ignore_case(country_code, "HK") ||
        ascii_equals_ignore_case(country_code, "MO") ||
        ascii_equals_ignore_case(country_code, "TW")) {
        return true;
    }

    const char *texts[] = {country, region, city};
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i) {
        const char *text = texts[i];
        if (text == NULL) {
            continue;
        }
        if (strstr(text, "香港") != NULL || strstr(text, "澳门") != NULL ||
            strstr(text, "澳門") != NULL || strstr(text, "台湾") != NULL ||
            strstr(text, "台灣") != NULL || strstr(text, "Hong Kong") != NULL ||
            strstr(text, "Macau") != NULL || strstr(text, "Macao") != NULL ||
            strstr(text, "Taiwan") != NULL) {
            return true;
        }
    }
    return false;
}

static bool weather_geo_is_mainland_cn(const char *country_code, const char *country,
                                       const char *region, const char *city)
{
    if (weather_geo_is_excluded_region(country_code, country, region, city)) {
        return false;
    }
    if (ascii_equals_ignore_case(country_code, "CN")) {
        return true;
    }
    return (country != NULL && (strstr(country, "中国") != NULL || strstr(country, "China") != NULL));
}

static const char *json_string_item(cJSON *object, const char *key)
{
    if (object == NULL || key == NULL) {
        return NULL;
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0') {
        return NULL;
    }
    return item->valuestring;
}

static const char *json_array_string(cJSON *array, int index)
{
    if (!cJSON_IsArray(array)) {
        return NULL;
    }

    cJSON *item = cJSON_GetArrayItem(array, index);
    if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0') {
        return NULL;
    }
    return item->valuestring;
}

static const char *json_first_string(cJSON *primary, cJSON *secondary,
                                     const char *key0, const char *key1, const char *key2)
{
    const char *keys[] = {key0, key1, key2};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        if (keys[i] == NULL) {
            continue;
        }

        const char *value = json_string_item(primary, keys[i]);
        if (value != NULL) {
            return value;
        }
        value = json_string_item(secondary, keys[i]);
        if (value != NULL) {
            return value;
        }
    }
    return NULL;
}

static bool weather_geo_prepare_city(const char *city, weather_geo_info_t *geo)
{
    if (city == NULL || geo == NULL) {
        return false;
    }

    char raw[WEATHER_QUERY_CITY_LEN] = {0};
    strlcpy(raw, city, sizeof(raw));
    char *trimmed = trim_ascii(raw);
    if (trimmed[0] == '\0' || strcmp(trimmed, "XX") == 0) {
        return false;
    }

    const char *alias = weather_city_ascii_alias(trimmed);
    if (alias != NULL) {
        strlcpy(geo->city_query, alias, sizeof(geo->city_query));
        strlcpy(geo->city_display, alias, sizeof(geo->city_display));
        return true;
    }

    char ascii_city[WEATHER_CITY_LEN] = {0};
    sanitize_weather_city(trimmed, ascii_city, sizeof(ascii_city));
    if (ascii_city[0] != '\0') {
        strlcpy(geo->city_query, ascii_city, sizeof(geo->city_query));
        strlcpy(geo->city_display, ascii_city, sizeof(geo->city_display));
        return true;
    }

    strlcpy(geo->city_query, trimmed, sizeof(geo->city_query));
    strlcpy(geo->city_display, "LOCAL", sizeof(geo->city_display));
    return true;
}

static bool weather_parse_cn_geo_response(const char *response, weather_geo_info_t *geo)
{
    if (response == NULL || geo == NULL) {
        return false;
    }

    memset(geo, 0, sizeof(*geo));
    cJSON *root = cJSON_Parse(response);
    if (root == NULL) {
        return false;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(data)) {
        data = root;
    }

    const char *country_code = json_first_string(data, root, "country_id", "countryCode", "country_code");
    const char *country = json_first_string(data, root, "country", NULL, NULL);
    const char *region = json_first_string(data, root, "region", "province", NULL);
    const char *city = json_first_string(data, root, "city", "cityName", NULL);
    const char *query_ip = json_first_string(data, root, "queryIp", "query", "ip");
    if (query_ip == NULL) {
        query_ip = json_first_string(data, root, "addr", NULL, NULL);
    }

    cJSON *location = cJSON_GetObjectItemCaseSensitive(data, "location");
    if (country == NULL) {
        country = json_array_string(location, 0);
    }
    if (region == NULL) {
        region = json_array_string(location, 1);
    }
    if (city == NULL) {
        city = json_array_string(location, 2);
    }

    bool is_mainland_cn = weather_geo_is_mainland_cn(country_code, country, region, city);
    if (is_mainland_cn && city == NULL) {
        city = region;
    }
    if (country_code != NULL) {
        strlcpy(geo->country_code, country_code, sizeof(geo->country_code));
    } else if (is_mainland_cn) {
        strlcpy(geo->country_code, "CN", sizeof(geo->country_code));
    }
    if (region != NULL) {
        strlcpy(geo->region_display, region, sizeof(geo->region_display));
    }
    if (city != NULL) {
        strlcpy(geo->city_native, city, sizeof(geo->city_native));
    }
    if (query_ip != NULL) {
        strlcpy(geo->query_ip, query_ip, sizeof(geo->query_ip));
    }

    geo->valid = is_mainland_cn && weather_geo_prepare_city(city, geo);
    cJSON_Delete(root);
    return geo->valid;
}

static bool url_encode_component(const char *input, char *output, size_t output_len)
{
    static const char hex[] = "0123456789ABCDEF";
    if (input == NULL || output == NULL || output_len == 0) {
        return false;
    }

    size_t out = 0;
    for (const unsigned char *p = (const unsigned char *)input; *p != '\0'; ++p) {
        bool unreserved = isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~';
        if (unreserved) {
            if (out + 1 >= output_len) {
                return false;
            }
            output[out++] = (char)*p;
        } else {
            if (out + 3 >= output_len) {
                return false;
            }
            output[out++] = '%';
            output[out++] = hex[*p >> 4];
            output[out++] = hex[*p & 0x0F];
        }
    }
    output[out] = '\0';
    return true;
}

static bool weather_build_city_url(const char *city_query, char *url, size_t url_len)
{
    char encoded_city[WEATHER_QUERY_CITY_LEN * 3] = {0};
    if (!url_encode_component(city_query, encoded_city, sizeof(encoded_city))) {
        return false;
    }

    int written = snprintf(url, url_len,
                           "http://wttr.in/%s?format=%%25l%%7C%%25t%%7C%%25h%%7C%%25C",
                           encoded_city);
    return written > 0 && (size_t)written < url_len;
}

static bool weather_build_open_meteo_geocode_url(const char *city_query, char *url, size_t url_len)
{
    char encoded_city[WEATHER_QUERY_CITY_LEN * 3] = {0};
    if (!url_encode_component(city_query, encoded_city, sizeof(encoded_city))) {
        return false;
    }

    int written = snprintf(url, url_len,
                           "http://geocoding-api.open-meteo.com/v1/search?name=%s&count=10&language=zh&format=json",
                           encoded_city);
    return written > 0 && (size_t)written < url_len;
}

static bool weather_build_open_meteo_forecast_url(double latitude, double longitude,
                                                  char *url, size_t url_len)
{
    int written = snprintf(url, url_len,
                           "http://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
                           "&current=temperature_2m,relative_humidity_2m,weather_code"
                           "&daily=weather_code,temperature_2m_max,temperature_2m_min"
                           "&forecast_days=7&timezone=auto",
                           latitude, longitude);
    return written > 0 && (size_t)written < url_len;
}

static bool json_value_to_int(cJSON *item, int *value)
{
    if (item == NULL || value == NULL) {
        return false;
    }
    if (cJSON_IsNumber(item)) {
        *value = (int)round(item->valuedouble);
        return true;
    }
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        return parse_first_int(item->valuestring, value);
    }
    return false;
}

static bool json_array_int(cJSON *array, int index, int *value)
{
    if (!cJSON_IsArray(array)) {
        return false;
    }
    return json_value_to_int(cJSON_GetArrayItem(array, index), value);
}

static const char *weather_condition_from_wmo(int code)
{
    if (code == 0) {
        return "CLEAR";
    }
    if (code == 1 || code == 2 || code == 3) {
        return "CLOUD";
    }
    if (code == 45 || code == 48) {
        return "FOG";
    }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return "RAIN";
    }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        return "SNOW";
    }
    if (code == 95 || code == 96 || code == 99) {
        return "THUNDER";
    }
    return "WEATHER";
}

static int weather_geocode_score(cJSON *result, const weather_geo_info_t *geo)
{
    int score = 0;
    const char *country_code = json_string_item(result, "country_code");
    const char *name = json_string_item(result, "name");
    const char *admin1 = json_string_item(result, "admin1");

    if (ascii_equals_ignore_case(country_code, "CN")) {
        score += 20;
    }
    if (geo != NULL && geo->region_display[0] != '\0' && admin1 != NULL &&
        (strstr(admin1, geo->region_display) != NULL ||
         strstr(geo->region_display, admin1) != NULL)) {
        score += 100;
    }
    if (geo != NULL && geo->city_native[0] != '\0' && name != NULL &&
        (strstr(name, geo->city_native) != NULL || strstr(geo->city_native, name) != NULL)) {
        score += 40;
    }

    cJSON *population = cJSON_GetObjectItemCaseSensitive(result, "population");
    if (cJSON_IsNumber(population) && population->valuedouble > 1000000.0) {
        score += 5;
    }
    return score;
}

static bool weather_parse_open_meteo_geocode_response(const char *response,
                                                      const weather_geo_info_t *geo,
                                                      double *latitude, double *longitude,
                                                      char *city, size_t city_len)
{
    if (response == NULL || latitude == NULL || longitude == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    if (root == NULL) {
        return false;
    }

    cJSON *results = cJSON_GetObjectItemCaseSensitive(root, "results");
    if (!cJSON_IsArray(results)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *best = NULL;
    int best_score = -1;
    int count = cJSON_GetArraySize(results);
    for (int i = 0; i < count; ++i) {
        cJSON *item = cJSON_GetArrayItem(results, i);
        if (!cJSON_IsObject(item)) {
            continue;
        }
        cJSON *lat = cJSON_GetObjectItemCaseSensitive(item, "latitude");
        cJSON *lon = cJSON_GetObjectItemCaseSensitive(item, "longitude");
        if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
            continue;
        }

        int score = weather_geocode_score(item, geo);
        if (score > best_score) {
            best = item;
            best_score = score;
        }
    }

    bool ok = false;
    if (best != NULL) {
        cJSON *lat = cJSON_GetObjectItemCaseSensitive(best, "latitude");
        cJSON *lon = cJSON_GetObjectItemCaseSensitive(best, "longitude");
        *latitude = lat->valuedouble;
        *longitude = lon->valuedouble;
        if (city != NULL && city_len > 0) {
            const char *name = json_string_item(best, "name");
            if (name != NULL) {
                strlcpy(city, name, city_len);
            } else if (geo != NULL && geo->city_display[0] != '\0') {
                strlcpy(city, geo->city_display, city_len);
            }
        }
        ok = true;
    }

    cJSON_Delete(root);
    return ok;
}

static bool weather_parse_open_meteo_forecast_response(const char *response, weather_info_t *out)
{
    if (response == NULL || out == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    if (root == NULL) {
        return false;
    }

    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    if (!cJSON_IsObject(current) || !cJSON_IsObject(daily)) {
        cJSON_Delete(root);
        return false;
    }

    int temp_c = 0;
    int humidity = 0;
    int weather_code = 0;
    if (!json_value_to_int(cJSON_GetObjectItemCaseSensitive(current, "temperature_2m"), &temp_c) ||
        !json_value_to_int(cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m"), &humidity) ||
        !json_value_to_int(cJSON_GetObjectItemCaseSensitive(current, "weather_code"), &weather_code)) {
        cJSON_Delete(root);
        return false;
    }

    out->valid = true;
    out->temp_c = temp_c;
    out->humidity = humidity;
    out->weather_code = weather_code;
    time(&out->updated_at);
    strlcpy(out->condition, weather_condition_from_wmo(weather_code), sizeof(out->condition));

    cJSON *dates = cJSON_GetObjectItemCaseSensitive(daily, "time");
    cJSON *codes = cJSON_GetObjectItemCaseSensitive(daily, "weather_code");
    cJSON *max_temps = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max");
    cJSON *min_temps = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min");
    int count = cJSON_IsArray(dates) ? cJSON_GetArraySize(dates) : 0;
    if (count > WEATHER_FORECAST_DAYS) {
        count = WEATHER_FORECAST_DAYS;
    }

    out->forecast_count = 0;
    for (int i = 0; i < count; ++i) {
        const char *date = json_array_string(dates, i);
        int code = 0;
        int max_c = 0;
        int min_c = 0;
        if (date == NULL ||
            !json_array_int(codes, i, &code) ||
            !json_array_int(max_temps, i, &max_c) ||
            !json_array_int(min_temps, i, &min_c)) {
            continue;
        }

        weather_info_t *info = out;
        int idx = info->forecast_count++;
        info->forecast[idx].valid = true;
        info->forecast[idx].weather_code = code;
        info->forecast[idx].min_c = min_c;
        info->forecast[idx].max_c = max_c;
        strlcpy(info->forecast[idx].date, date, sizeof(info->forecast[idx].date));
        strlcpy(info->forecast[idx].condition, weather_condition_from_wmo(code),
                sizeof(info->forecast[idx].condition));
    }

    cJSON_Delete(root);
    return true;
}

static const char *weather_condition_zh(const char *condition)
{
    if (condition == NULL) {
        return "天气";
    }
    if (strstr(condition, "THUNDER") != NULL || strstr(condition, "STORM") != NULL) {
        return "雷雨";
    }
    if (strstr(condition, "RAIN") != NULL || strstr(condition, "SHOWER") != NULL) {
        return "雨";
    }
    if (strstr(condition, "SNOW") != NULL) {
        return "雪";
    }
    if (strstr(condition, "FOG") != NULL || strstr(condition, "MIST") != NULL ||
        strstr(condition, "HAZE") != NULL) {
        return "雾";
    }
    if (strstr(condition, "CLOUD") != NULL || strstr(condition, "OVERCAST") != NULL) {
        return "多云";
    }
    if (strstr(condition, "SUN") != NULL || strstr(condition, "CLEAR") != NULL) {
        return "晴";
    }
    return "天气";
}

static bool weather_parse_response(const char *response, weather_info_t *out)
{
    if (response == NULL || out == NULL) {
        return false;
    }

    char buf[WEATHER_HTTP_BUFFER_SIZE] = {0};
    strlcpy(buf, response, sizeof(buf));

    char *saveptr = NULL;
    char *first_text = strtok_r(buf, "|", &saveptr);
    char *second_text = strtok_r(NULL, "|", &saveptr);
    char *third_text = strtok_r(NULL, "|", &saveptr);
    char *fourth_text = strtok_r(NULL, "|", &saveptr);

    char *city_text = NULL;
    char *temp_text = first_text;
    char *hum_text = second_text;
    char *condition_text = third_text;
    if (fourth_text != NULL) {
        city_text = first_text;
        temp_text = second_text;
        hum_text = third_text;
        condition_text = fourth_text;
    }

    if (temp_text == NULL || hum_text == NULL || condition_text == NULL) {
        return false;
    }
    if (city_text != NULL) {
        city_text = trim_ascii(city_text);
    }
    temp_text = trim_ascii(temp_text);
    hum_text = trim_ascii(hum_text);
    condition_text = trim_ascii(condition_text);

    int temp_c = 0;
    int humidity = 0;
    if (!parse_first_int(temp_text, &temp_c) || !parse_first_int(hum_text, &humidity)) {
        return false;
    }

    out->valid = true;
    out->weather_code = -1;
    out->temp_c = temp_c;
    out->humidity = humidity;
    out->forecast_count = 0;
    time(&out->updated_at);
    sanitize_weather_city(city_text, out->city, sizeof(out->city));
    sanitize_weather_condition(condition_text, out->condition, sizeof(out->condition));
    return true;
}

static esp_err_t weather_http_event_handler(esp_http_client_event_t *evt)
{
    weather_http_buffer_t *buffer = (weather_http_buffer_t *)evt->user_data;
    if (evt->event_id != HTTP_EVENT_ON_DATA || buffer == NULL || evt->data == NULL || evt->data_len <= 0) {
        return ESP_OK;
    }

    int remain = buffer->cap - buffer->len - 1;
    if (remain <= 0) {
        return ESP_OK;
    }

    int copy_len = evt->data_len;
    if (copy_len > remain) {
        copy_len = remain;
    }
    memcpy(buffer->data + buffer->len, evt->data, (size_t)copy_len);
    buffer->len += copy_len;
    buffer->data[buffer->len] = '\0';
    return ESP_OK;
}

static esp_err_t http_get_text(const char *url, char *response, size_t response_len, const char *label)
{
    ESP_RETURN_ON_FALSE(url != NULL && url[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "invalid HTTP URL");
    ESP_RETURN_ON_FALSE(response != NULL && response_len > 1, ESP_ERR_INVALID_ARG, TAG, "invalid HTTP buffer");

    memset(response, 0, response_len);
    weather_http_buffer_t buffer = {
        .data = response,
        .len = 0,
        .cap = (int)response_len,
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 8000,
        .event_handler = weather_http_event_handler,
        .user_data = &buffer,
        .user_agent = "esp32-cyd-clock",
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ESP_RETURN_ON_FALSE(client != NULL, ESP_ERR_NO_MEM, TAG, "init weather HTTP client failed");

    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (ret != ESP_OK || status < 200 || status >= 300 || buffer.len <= 0) {
        ESP_LOGW(TAG, "%s HTTP failed: ret=%s status=%d len=%d url=%s",
                 label != NULL ? label : "http", esp_err_to_name(ret), status, buffer.len, url);
        if (ret == ESP_OK) {
            ret = ESP_ERR_INVALID_RESPONSE;
        }
    }

    esp_http_client_cleanup(client);
    return ret;
}

static esp_err_t weather_fetch_url(const char *url, weather_info_t *out, const char *label)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid weather output");

    char response[WEATHER_HTTP_BUFFER_SIZE] = {0};
    esp_err_t ret = http_get_text(url, response, sizeof(response), label);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!weather_parse_response(response, out)) {
        ESP_LOGW(TAG, "%s parse failed: %s", label != NULL ? label : "weather", response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t weather_fetch_open_meteo_city(const weather_geo_info_t *geo, weather_info_t *out)
{
    ESP_RETURN_ON_FALSE(geo != NULL && out != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "invalid Open-Meteo request");

    const char *city_query = geo->city_native[0] != '\0' ? geo->city_native : geo->city_query;
    if (city_query == NULL || city_query[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char url[WEATHER_URL_BUFFER_SIZE] = {0};
    if (!weather_build_open_meteo_geocode_url(city_query, url, sizeof(url))) {
        return ESP_ERR_INVALID_SIZE;
    }

    char response[WEATHER_HTTP_BUFFER_SIZE] = {0};
    esp_err_t ret = http_get_text(url, response, sizeof(response), "meteo-geo");
    if (ret != ESP_OK) {
        return ret;
    }

    double latitude = 0.0;
    double longitude = 0.0;
    char city[WEATHER_CITY_LEN] = {0};
    if (!weather_parse_open_meteo_geocode_response(response, geo, &latitude, &longitude,
                                                   city, sizeof(city))) {
        ESP_LOGW(TAG, "Open-Meteo geocode parse failed: city=%s response=%.120s",
                 city_query, response);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (!weather_build_open_meteo_forecast_url(latitude, longitude, url, sizeof(url))) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(response, 0, sizeof(response));
    ret = http_get_text(url, response, sizeof(response), "meteo-weather");
    if (ret != ESP_OK) {
        return ret;
    }

    weather_info_t parsed = {0};
    parsed.weather_code = -1;
    if (!weather_parse_open_meteo_forecast_response(response, &parsed)) {
        ESP_LOGW(TAG, "Open-Meteo weather parse failed: %.120s", response);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (geo->city_display[0] != '\0') {
        strlcpy(parsed.city, geo->city_display, sizeof(parsed.city));
    } else if (city[0] != '\0') {
        sanitize_weather_city(city, parsed.city, sizeof(parsed.city));
    }

    ESP_LOGI(TAG, "Open-Meteo selected: city=%s lat=%.4f lon=%.4f days=%d",
             parsed.city[0] != '\0' ? parsed.city : city_query,
             latitude, longitude, parsed.forecast_count);
    *out = parsed;
    return ESP_OK;
}

static esp_err_t weather_fetch_preferred_cn(weather_info_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid weather output");
    if (strlen(CONFIG_CLOCK_WEATHER_CN_GEO_URL) == 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    char geo_response[WEATHER_GEO_BUFFER_SIZE] = {0};
    esp_err_t ret = http_get_text(CONFIG_CLOCK_WEATHER_CN_GEO_URL, geo_response,
                                  sizeof(geo_response), "cn-geo");
    if (ret != ESP_OK) {
        return ret;
    }

    weather_geo_info_t geo = {0};
    if (!weather_parse_cn_geo_response(geo_response, &geo)) {
        ESP_LOGI(TAG, "cn geo not preferred: %.120s", geo_response);
        return ESP_ERR_INVALID_RESPONSE;
    }

    char weather_url[WEATHER_URL_BUFFER_SIZE] = {0};
    if (!weather_build_city_url(geo.city_query, weather_url, sizeof(weather_url))) {
        ESP_LOGW(TAG, "build preferred weather URL failed: city=%s", geo.city_query);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "cn geo selected: ip=%s country=%s city=%s display=%s",
             geo.query_ip[0] != '\0' ? geo.query_ip : "-",
             geo.country_code[0] != '\0' ? geo.country_code : "-",
             geo.city_query, geo.city_display);

    weather_info_t parsed = {0};
    ret = weather_fetch_open_meteo_city(&geo, &parsed);
    if (ret == ESP_OK) {
        *out = parsed;
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Open-Meteo failed: %s, fallback to wttr city weather",
             esp_err_to_name(ret));
    ret = weather_fetch_url(weather_url, &parsed, "weather-cn");
    if (ret != ESP_OK) {
        return ret;
    }
    if (parsed.city[0] == '\0' && geo.city_display[0] != '\0') {
        strlcpy(parsed.city, geo.city_display, sizeof(parsed.city));
    }

    *out = parsed;
    return ESP_OK;
}

static esp_err_t weather_fetch_once(void)
{
#if CONFIG_CLOCK_WEATHER_ENABLE
    weather_info_t parsed = {0};
    const char *source = "cn";
    esp_err_t ret = weather_fetch_preferred_cn(&parsed);
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGI(TAG, "preferred cn geo failed: %s, fallback to auto weather",
                     esp_err_to_name(ret));
        }
        source = "auto";
        ret = weather_fetch_url(CONFIG_CLOCK_WEATHER_URL, &parsed, "weather-auto");
    }

    if (ret == ESP_OK) {
        weather_set_snapshot(&parsed);
        ESP_LOGI(TAG, "weather updated[%s]: city=%s temp=%d humidity=%d condition=%s days=%d",
                 source, parsed.city[0] != '\0' ? parsed.city : "-",
                 parsed.temp_c, parsed.humidity, parsed.condition, parsed.forecast_count);
    }
    return ret;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static void weather_task(void *arg)
{
    (void)arg;
    const TickType_t refresh_ticks = pdMS_TO_TICKS(CONFIG_CLOCK_WEATHER_REFRESH_MINUTES * 60 * 1000);

    for (;;) {
        if (s_wifi_event_group == NULL) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                               pdFALSE, pdTRUE, pdMS_TO_TICKS(30000));
        if ((bits & WIFI_CONNECTED_BIT) == 0) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        (void)weather_fetch_once();
        vTaskDelay(refresh_ticks > 0 ? refresh_ticks : pdMS_TO_TICKS(600000));
    }
}

static const char *weekday_name(int wday)
{
    static const char *names[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    if (wday < 0 || wday > 6) {
        return "星期天";
    }
    return names[wday];
}

static esp_err_t draw_wifi_icon(int x, int y, bool active)
{
    const uint16_t color = active ? rgb565(64, 235, 255) : rgb565(82, 93, 125);
    const int cx = x + 12;
    const int cy = y + 16;
    const int radii[] = {14, 10, 6};

    for (size_t r = 0; r < sizeof(radii) / sizeof(radii[0]); ++r) {
        for (int deg = 205; deg <= 335; deg += 10) {
            float rad = (float)deg * (float)M_PI / 180.0f;
            int px = cx + (int)roundf(cosf(rad) * radii[r]);
            int py = cy + (int)roundf(sinf(rad) * radii[r]);
            ESP_RETURN_ON_ERROR(lcd_fill_circle(px, py, 1, color), TAG, "draw wifi arc failed");
        }
    }
    return lcd_fill_circle(cx, cy, 2, color);
}

static esp_err_t draw_sun_icon(int cx, int cy, int radius, uint16_t color)
{
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, radius, color), TAG, "draw sun failed");
    for (int i = 0; i < 8; ++i) {
        float a = (float)i * (float)M_PI / 4.0f;
        int x0 = cx + (int)roundf(cosf(a) * (radius + 4));
        int y0 = cy + (int)roundf(sinf(a) * (radius + 4));
        int x1 = cx + (int)roundf(cosf(a) * (radius + 9));
        int y1 = cy + (int)roundf(sinf(a) * (radius + 9));
        ESP_RETURN_ON_ERROR(lcd_draw_line(x0, y0, x1, y1, 2, color), TAG, "draw sun ray failed");
    }
    return ESP_OK;
}

static esp_err_t draw_dashboard_weather_icon(const char *condition, int cx, int cy,
                                             uint16_t accent, uint16_t muted)
{
    const uint16_t cloud = rgb565(204, 224, 238);
    const uint16_t shade = rgb565(128, 154, 188);
    const uint16_t rain = rgb565(72, 206, 255);
    const uint16_t snow = rgb565(238, 250, 255);
    const bool has_condition = condition != NULL && condition[0] != '\0' &&
                               strstr(condition, "WAIT") == NULL &&
                               strstr(condition, "WEATHER") == NULL;
    const bool thunder = has_condition &&
                         (strstr(condition, "THUNDER") != NULL || strstr(condition, "STORM") != NULL);
    const bool rainy = has_condition &&
                       (strstr(condition, "RAIN") != NULL || strstr(condition, "SHOWER") != NULL);
    const bool snowy = has_condition && strstr(condition, "SNOW") != NULL;
    const bool cloudy = has_condition &&
                        (strstr(condition, "CLOUD") != NULL || strstr(condition, "OVERCAST") != NULL);
    const bool foggy = has_condition &&
                       (strstr(condition, "FOG") != NULL || strstr(condition, "MIST") != NULL ||
                        strstr(condition, "HAZE") != NULL);

    if (!has_condition) {
        ESP_RETURN_ON_ERROR(lcd_draw_hline(cx - 7, cy - 2, 15, muted),
                            TAG, "draw unknown weather failed");
        return lcd_draw_hline(cx - 5, cy + 3, 11, muted);
    }

    if (!rainy && !snowy && !cloudy && !foggy && !thunder) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 5, accent), TAG, "draw dashboard sun failed");
        for (int i = 0; i < 8; ++i) {
            float a = (float)i * (float)M_PI / 4.0f;
            int x0 = cx + (int)roundf(cosf(a) * 8.0f);
            int y0 = cy + (int)roundf(sinf(a) * 8.0f);
            int x1 = cx + (int)roundf(cosf(a) * 11.0f);
            int y1 = cy + (int)roundf(sinf(a) * 11.0f);
            ESP_RETURN_ON_ERROR(lcd_draw_line(x0, y0, x1, y1, 1, accent),
                                TAG, "draw dashboard sun ray failed");
        }
        return ESP_OK;
    }

    // 首页右上角空间较小，云体尺寸要控制在温度文字左侧。
    if (cloudy && !rainy && !snowy && !foggy && !thunder) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 8, cy - 5, 4, accent),
                            TAG, "draw dashboard cloud sun failed");
    }
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 7, cy + 2, 5, shade),
                        TAG, "draw dashboard cloud shade failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy - 2, 7, cloud),
                        TAG, "draw dashboard cloud body failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx + 8, cy + 2, 5, cloud),
                        TAG, "draw dashboard cloud body failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(cx - 12, cy + 2, 25, 7, 3, cloud),
                        TAG, "draw dashboard cloud base failed");

    if (thunder) {
        return lcd_fill_triangle(cx, cy + 7, cx - 5, cy + 15, cx + 3, cy + 12,
                                 rgb565(255, 216, 73));
    }
    if (rainy) {
        ESP_RETURN_ON_ERROR(lcd_draw_line(cx - 6, cy + 10, cx - 9, cy + 15, 1, rain),
                            TAG, "draw dashboard rain failed");
        ESP_RETURN_ON_ERROR(lcd_draw_line(cx, cy + 10, cx - 3, cy + 16, 1, rain),
                            TAG, "draw dashboard rain failed");
        return lcd_draw_line(cx + 6, cy + 10, cx + 3, cy + 15, 1, rain);
    }
    if (snowy) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 6, cy + 13, 1, snow),
                            TAG, "draw dashboard snow failed");
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy + 15, 1, snow),
                            TAG, "draw dashboard snow failed");
        return lcd_fill_circle(cx + 6, cy + 13, 1, snow);
    }
    if (foggy) {
        ESP_RETURN_ON_ERROR(lcd_draw_hline(cx - 12, cy + 11, 25, muted),
                            TAG, "draw dashboard fog failed");
        return lcd_draw_hline(cx - 9, cy + 15, 19, muted);
    }
    return ESP_OK;
}

static esp_err_t draw_card_icon(int kind, int cx, int cy, uint16_t accent)
{
    const uint16_t icon_bg = mix_color(rgb565(8, 12, 33), accent, 2, 5);
    const uint16_t white = rgb565(216, 244, 255);

    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 13, icon_bg), TAG, "draw icon bg failed");
    if (kind == 0) {
        ESP_RETURN_ON_ERROR(lcd_draw_rect_outline(cx - 7, cy - 7, 14, 14, accent), TAG,
                            "draw clock outline failed");
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 2, white), TAG, "draw clock center failed");
        ESP_RETURN_ON_ERROR(lcd_draw_line(cx, cy, cx, cy - 6, 2, white), TAG, "draw clock hand failed");
        return lcd_draw_line(cx, cy, cx + 6, cy + 4, 2, white);
    }
    if (kind == 1) {
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(cx - 8, cy - 8, 16, 15, 3, accent), TAG,
                            "draw calendar failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - 5, cy - 3, 10, 2, icon_bg), TAG, "draw calendar line failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - 5, cy + 2, 8, 2, icon_bg), TAG, "draw calendar line failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - 5, cy - 11, 3, 6, white), TAG, "draw calendar pin failed");
        return lcd_fill_rect(cx + 3, cy - 11, 3, 6, white);
    }

    ESP_RETURN_ON_ERROR(lcd_fill_triangle(cx, cy - 12, cx - 8, cy + 2, cx + 8, cy + 2, accent),
                        TAG, "draw drop top failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy + 3, 8, accent), TAG, "draw drop body failed");
    return lcd_draw_line(cx + 4, cy + 7, cx + 7, cy + 3, 1, rgb565(10, 78, 92));
}

static esp_err_t draw_metric_card(int x, int y, int w, int h,
                                  const char *label, const char *value,
                                  int value_scale, uint16_t accent, int icon_kind)
{
    const uint16_t border = mix_color(rgb565(12, 21, 50), accent, 1, 2);
    const uint16_t card = mix_color(rgb565(5, 10, 28), accent, 1, 6);
    const uint16_t label_color = rgb565(210, 225, 244);
    int scale = value_scale;

    if (text5_width(value, scale) > w - 10) {
        scale = 1;
    }

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, w, h, 8, border), TAG, "draw metric card border failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 1, y + 1, w - 2, h - 2, 7, card),
                        TAG, "draw metric card failed");
    ESP_RETURN_ON_ERROR(draw_card_icon(icon_kind, x + 17, y + 15, accent), TAG, "draw card icon failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(x + 35, y + 7, label, w - 39, 1, label_color),
                        TAG, "draw metric label failed");

    int value_x = x + (w - text5_width(value, scale)) / 2;
    int value_y = y + h - (scale > 1 ? 18 : 13);
    if (value_x < x + 4) {
        value_x = x + 4;
    }
    return draw_text5_limited(value_x, value_y, value, 9, scale, accent);
}

static uint16_t nav_accent(app_view_t current_view, app_view_t target_view,
                           uint16_t normal, uint16_t active)
{
    return current_view == target_view ? active : normal;
}

static esp_err_t draw_nav_cards(const struct tm *tm, app_view_t current_view,
                                const weather_info_t *weather)
{
    char card_time_buf[32];
    char short_date_buf[32];
    char hum_buf[16];

    snprintf(card_time_buf, sizeof(card_time_buf), "%02d:%02d:%02d",
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    snprintf(short_date_buf, sizeof(short_date_buf), "%02d/%02d", tm->tm_mon + 1, tm->tm_mday);
    if (weather != NULL && weather->valid) {
        snprintf(hum_buf, sizeof(hum_buf), "%d%%", weather->humidity);
    } else {
        strlcpy(hum_buf, "--%", sizeof(hum_buf));
    }

    ESP_RETURN_ON_ERROR(draw_metric_card(NAV_CARD_X0, NAV_CARD_Y, NAV_CARD_W, NAV_CARD_H,
                                         "时间", card_time_buf, 1,
                                         nav_accent(current_view, APP_VIEW_ANALOG_CLOCK,
                                                    rgb565(45, 147, 255), rgb565(105, 207, 255)),
                                         0),
                        TAG, "draw time nav failed");
    ESP_RETURN_ON_ERROR(draw_metric_card(NAV_CARD_X1, NAV_CARD_Y, NAV_CARD_W, NAV_CARD_H,
                                         "日期", short_date_buf, 2,
                                         nav_accent(current_view, APP_VIEW_CALENDAR,
                                                    rgb565(186, 85, 255), rgb565(227, 124, 255)),
                                         1),
                        TAG, "draw date nav failed");
    return draw_metric_card(NAV_CARD_X2, NAV_CARD_Y, NAV_CARD_W, NAV_CARD_H,
                            "湿度", hum_buf, 2,
                            nav_accent(current_view, APP_VIEW_WEATHER,
                                       rgb565(30, 226, 214), rgb565(95, 255, 221)),
                            2);
}

static esp_err_t draw_clock_screen(const struct tm *tm)
{
    char date_buf[32];
    char temp_buf[16];
    const char *condition_text = "天气";
    weather_info_t weather = {0};
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(141, 153, 190);
    const uint16_t yellow = rgb565(255, 211, 85);
    const char *status_text = s_prov_active ? "配网" :
                              (!s_wifi_configured ? "未配网" :
                               (s_wifi_connected && s_connected_ssid[0] != '\0' ? s_connected_ssid : "连接中"));
    const char *center_text = s_time_synced ? CONFIG_CLOCK_WEATHER_CITY_LABEL : "对时中";

    snprintf(date_buf, sizeof(date_buf), "%04d/%02d/%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    weather_get_snapshot(&weather);
    if (weather.valid) {
        snprintf(temp_buf, sizeof(temp_buf), "%dC", weather.temp_c);
        condition_text = weather_condition_zh(weather.condition);
        if (weather.city[0] != '\0') {
            center_text = weather.city;
        }
    } else {
        strlcpy(temp_buf, "--C", sizeof(temp_buf));
        condition_text = s_prov_active ? "配网" : "天气";
    }

    // 先在内存中合成完整面板，再分块刷屏，避免清屏和逐元素重画被人眼看到。
    ESP_RETURN_ON_ERROR(lcd_canvas_begin(CLOCK_PANEL_X, CLOCK_PANEL_Y, CLOCK_PANEL_W, CLOCK_PANEL_H),
                        TAG, "begin canvas failed");
    ESP_RETURN_ON_ERROR(lcd_canvas_fill_gradient(rgb565(4, 7, 21), rgb565(5, 9, 28)),
                        TAG, "fill canvas failed");
    ESP_RETURN_ON_ERROR(draw_panel_frame(), TAG, "draw panel failed");
    ESP_RETURN_ON_ERROR(draw_scenic_background(), TAG, "draw scenic background failed");

    ESP_RETURN_ON_ERROR(draw_wifi_icon(CLOCK_PANEL_X + 12, CLOCK_PANEL_Y + 8,
                                       s_wifi_connected || s_prov_active),
                        TAG, "draw wifi icon failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(CLOCK_PANEL_X + 42, CLOCK_PANEL_Y + 10, status_text,
                                                120, 1, s_prov_active ? yellow : text),
                        TAG, "draw status text failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_centered_limited(160, CLOCK_PANEL_Y + 25, center_text,
                                                         150, 1, muted),
                        TAG, "draw center text failed");
    int temp_x = CLOCK_PANEL_X + CLOCK_PANEL_W - text5_width(temp_buf, 2) - 10;
    int weather_icon_x = temp_x - 18;
    ESP_RETURN_ON_ERROR(draw_dashboard_weather_icon(weather.valid ? weather.condition : NULL,
                                                    weather_icon_x, CLOCK_PANEL_Y + 23,
                                                    yellow, muted),
                        TAG, "draw dashboard weather icon failed");
    ESP_RETURN_ON_ERROR(draw_text5(temp_x, CLOCK_PANEL_Y + 17, temp_buf, 2, text),
                        TAG, "draw temperature failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(CLOCK_PANEL_X + CLOCK_PANEL_W - 43,
                                                CLOCK_PANEL_Y + 37, condition_text,
                                                40, 1, muted),
                        TAG, "draw weather text failed");

    ESP_RETURN_ON_ERROR(draw_time_digits(tm), TAG, "draw time failed");

    ESP_RETURN_ON_ERROR(draw_text5(CLOCK_PANEL_X + 18, CLOCK_PANEL_Y + 114, date_buf, 2, text),
                        TAG, "draw date failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed(CLOCK_PANEL_X + 18, CLOCK_PANEL_Y + 132,
                                        weekday_name(tm->tm_wday), 1, text),
                        TAG, "draw weekday failed");
    ESP_RETURN_ON_ERROR(lcd_fill_vertical_blend_rect(CLOCK_PANEL_X + 18, CLOCK_PANEL_Y + 150, 27, 3,
                                                     rgb565(62, 233, 255), rgb565(176, 76, 255)),
                        TAG, "draw date accent failed");

    ESP_RETURN_ON_ERROR(draw_nav_cards(tm, APP_VIEW_DASHBOARD, &weather), TAG, "draw nav cards failed");
    ESP_RETURN_ON_ERROR(draw_progress_dots(tm->tm_sec), TAG, "draw progress dots failed");
    return lcd_canvas_flush();
}

static esp_err_t draw_view_background(uint16_t top, uint16_t bottom)
{
    ESP_RETURN_ON_ERROR(lcd_canvas_begin(CLOCK_PANEL_X, CLOCK_PANEL_Y, CLOCK_PANEL_W, CLOCK_PANEL_H),
                        TAG, "begin view canvas failed");
    ESP_RETURN_ON_ERROR(lcd_canvas_fill_gradient(top, bottom), TAG, "fill view canvas failed");
    ESP_RETURN_ON_ERROR(draw_panel_frame(), TAG, "draw view panel failed");
    return lcd_fill_vertical_blend_rect(CLOCK_PANEL_X + 1, CLOCK_PANEL_Y + 1,
                                        CLOCK_PANEL_W - 2, 42,
                                        mix_color(top, rgb565(32, 54, 96), 1, 3), top);
}

static esp_err_t draw_view_header(const char *title, const struct tm *tm)
{
    char time_buf[8];
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(142, 158, 191);
    const uint16_t yellow = rgb565(255, 211, 85);
    const char *status_text = s_prov_active ? "配网" :
                              (!s_wifi_configured ? "未配网" :
                               (s_wifi_connected && s_connected_ssid[0] != '\0' ? s_connected_ssid : "连接中"));

    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
    ESP_RETURN_ON_ERROR(draw_wifi_icon(CLOCK_PANEL_X + 10, CLOCK_PANEL_Y + 5,
                                       s_wifi_connected || s_prov_active),
                        TAG, "draw view wifi failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(CLOCK_PANEL_X + 40, CLOCK_PANEL_Y + 9,
                                                status_text, 92, 1,
                                                s_prov_active ? yellow : muted),
                        TAG, "draw view wifi text failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_centered_limited(160, CLOCK_PANEL_Y + 9,
                                                         title, 74, 1, text),
                        TAG, "draw view title failed");
    return draw_text5(CLOCK_PANEL_X + CLOCK_PANEL_W - text5_width(time_buf, 1) - 12,
                      CLOCK_PANEL_Y + 14, time_buf, 1, muted);
}

static esp_err_t draw_clock_hand(int cx, int cy, float degrees, int length,
                                 int thickness, uint16_t color)
{
    float rad = degrees * (float)M_PI / 180.0f;
    int x = cx + (int)roundf(cosf(rad) * length);
    int y = cy + (int)roundf(sinf(rad) * length);
    return lcd_draw_line(cx, cy, x, y, thickness, color);
}

static esp_err_t draw_analog_clock_screen(const struct tm *tm)
{
    const int cx = 160;
    const int cy = 124;
    const int radius = 58;
    const uint16_t face = rgb565(233, 241, 244);
    const uint16_t ring = rgb565(71, 153, 255);
    const uint16_t dark = rgb565(16, 24, 42);
    const uint16_t tick = rgb565(53, 71, 100);
    const uint16_t minute_color = rgb565(31, 45, 73);
    const uint16_t second_color = rgb565(255, 95, 125);
    char digital_buf[16];

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(5, 9, 24), rgb565(7, 18, 35)),
                        TAG, "draw analog bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("时间", tm), TAG, "draw analog header failed");

    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, radius + 5, rgb565(10, 29, 58)),
                        TAG, "draw clock outer failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, radius + 2, ring), TAG, "draw clock ring failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, radius, face), TAG, "draw clock face failed");

    for (int i = 0; i < 60; ++i) {
        float degrees = (float)i * 6.0f - 90.0f;
        float rad = degrees * (float)M_PI / 180.0f;
        int outer = radius - 2;
        int inner = radius - ((i % 5) == 0 ? 10 : 5);
        int x0 = cx + (int)roundf(cosf(rad) * inner);
        int y0 = cy + (int)roundf(sinf(rad) * inner);
        int x1 = cx + (int)roundf(cosf(rad) * outer);
        int y1 = cy + (int)roundf(sinf(rad) * outer);
        ESP_RETURN_ON_ERROR(lcd_draw_line(x0, y0, x1, y1, (i % 5) == 0 ? 2 : 1, tick),
                            TAG, "draw clock tick failed");
    }

    ESP_RETURN_ON_ERROR(draw_text5_centered(cx, cy - radius + 12, "12", 2, dark),
                        TAG, "draw clock number failed");
    ESP_RETURN_ON_ERROR(draw_text5_centered(cx + radius - 17, cy - 7, "3", 2, dark),
                        TAG, "draw clock number failed");
    ESP_RETURN_ON_ERROR(draw_text5_centered(cx, cy + radius - 24, "6", 2, dark),
                        TAG, "draw clock number failed");
    ESP_RETURN_ON_ERROR(draw_text5_centered(cx - radius + 16, cy - 7, "9", 2, dark),
                        TAG, "draw clock number failed");

    float hour_degrees = ((float)(tm->tm_hour % 12) + (float)tm->tm_min / 60.0f +
                          (float)tm->tm_sec / 3600.0f) * 30.0f - 90.0f;
    float minute_degrees = ((float)tm->tm_min + (float)tm->tm_sec / 60.0f) * 6.0f - 90.0f;
    float second_degrees = (float)tm->tm_sec * 6.0f - 90.0f;
    ESP_RETURN_ON_ERROR(draw_clock_hand(cx, cy, hour_degrees, 31, 5, dark),
                        TAG, "draw hour hand failed");
    ESP_RETURN_ON_ERROR(draw_clock_hand(cx, cy, minute_degrees, 44, 3, minute_color),
                        TAG, "draw minute hand failed");
    ESP_RETURN_ON_ERROR(draw_clock_hand(cx, cy, second_degrees, 50, 1, second_color),
                        TAG, "draw second hand failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 5, rgb565(255, 255, 255)),
                        TAG, "draw clock center failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 2, second_color), TAG, "draw clock pin failed");

    snprintf(digital_buf, sizeof(digital_buf), "%02d:%02d:%02d",
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    ESP_RETURN_ON_ERROR(draw_text5_centered(160, CLOCK_PANEL_Y + 176, digital_buf, 2,
                                            rgb565(208, 239, 255)),
                        TAG, "draw analog digital failed");
    return lcd_canvas_flush();
}

static bool is_leap_year(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static int days_in_month(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}

static esp_err_t draw_calendar_screen(const struct tm *tm)
{
    static const char *week_labels[] = {"日", "一", "二", "三", "四", "五", "六"};
    const int year = tm->tm_year + 1900;
    const int month = tm->tm_mon + 1;
    const int cell_w = 36;
    const int row_h = 14;
    const int grid_x = 34;
    const int week_y = CLOCK_PANEL_Y + 66;
    const int day_y = CLOCK_PANEL_Y + 88;
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(140, 156, 190);
    const uint16_t line = rgb565(36, 51, 82);
    const uint16_t today_bg = rgb565(224, 106, 255);
    const uint16_t today_text = rgb565(15, 17, 32);
    char month_buf[32];
    struct tm first_day = {
        .tm_year = tm->tm_year,
        .tm_mon = tm->tm_mon,
        .tm_mday = 1,
        .tm_hour = 12,
    };

    (void)mktime(&first_day);
    int first_wday = first_day.tm_wday;
    int month_days = days_in_month(year, month);

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(7, 10, 25), rgb565(17, 14, 36)),
                        TAG, "draw calendar bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("日期", tm), TAG, "draw calendar header failed");

    snprintf(month_buf, sizeof(month_buf), "%04d/%02d", year, month);
    ESP_RETURN_ON_ERROR(draw_text5_centered(160, CLOCK_PANEL_Y + 42, month_buf, 2, text),
                        TAG, "draw month failed");
    ESP_RETURN_ON_ERROR(lcd_draw_hline(grid_x, CLOCK_PANEL_Y + 62, cell_w * 7, line),
                        TAG, "draw calendar line failed");

    for (int col = 0; col < 7; ++col) {
        int center_x = grid_x + col * cell_w + cell_w / 2;
        ESP_RETURN_ON_ERROR(draw_text_mixed_centered_limited(center_x, week_y,
                                                             week_labels[col], 18, 1, muted),
                            TAG, "draw weekday label failed");
    }

    for (int day = 1; day <= month_days; ++day) {
        int index = first_wday + day - 1;
        int col = index % 7;
        int row = index / 7;
        int center_x = grid_x + col * cell_w + cell_w / 2;
        int y = day_y + row * row_h;
        char day_buf[12];
        uint16_t day_color = text;

        snprintf(day_buf, sizeof(day_buf), "%d", day);
        if (day == tm->tm_mday) {
            ESP_RETURN_ON_ERROR(lcd_fill_round_rect(center_x - 13, y - 1, 26, 13, 4, today_bg),
                                TAG, "draw today highlight failed");
            day_color = today_text;
        } else if (col == 0 || col == 6) {
            day_color = rgb565(121, 206, 255);
        }
        ESP_RETURN_ON_ERROR(draw_text5_centered(center_x, y + 2, day_buf, 1, day_color),
                            TAG, "draw calendar day failed");
    }

    return lcd_canvas_flush();
}

static esp_err_t draw_cloud_symbol(int cx, int cy, uint16_t cloud, uint16_t shade)
{
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 15, cy + 4, 13, shade), TAG, "draw cloud shade failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy - 3, 17, cloud), TAG, "draw cloud body failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx + 17, cy + 5, 12, cloud), TAG, "draw cloud body failed");
    return lcd_fill_round_rect(cx - 27, cy + 4, 56, 18, 7, cloud);
}

static esp_err_t draw_weather_symbol(const char *condition, int cx, int cy)
{
    const uint16_t sun = rgb565(255, 203, 84);
    const uint16_t cloud = rgb565(216, 231, 241);
    const uint16_t shade = rgb565(132, 158, 189);
    const uint16_t rain = rgb565(72, 206, 255);
    const uint16_t snow = rgb565(238, 250, 255);
    bool thunder = condition != NULL &&
                   (strstr(condition, "THUNDER") != NULL || strstr(condition, "STORM") != NULL);
    bool rainy = condition != NULL &&
                 (strstr(condition, "RAIN") != NULL || strstr(condition, "SHOWER") != NULL);
    bool snowy = condition != NULL && strstr(condition, "SNOW") != NULL;
    bool cloudy = condition != NULL &&
                  (strstr(condition, "CLOUD") != NULL || strstr(condition, "OVERCAST") != NULL);
    bool foggy = condition != NULL &&
                 (strstr(condition, "FOG") != NULL || strstr(condition, "MIST") != NULL ||
                  strstr(condition, "HAZE") != NULL);

    if (!rainy && !snowy && !cloudy && !foggy && !thunder) {
        return draw_sun_icon(cx, cy, 18, sun);
    }

    if (cloudy || foggy) {
        ESP_RETURN_ON_ERROR(draw_sun_icon(cx - 19, cy - 8, 12, sun), TAG, "draw cloud sun failed");
    }
    ESP_RETURN_ON_ERROR(draw_cloud_symbol(cx, cy, cloud, shade), TAG, "draw cloud failed");

    if (thunder) {
        return lcd_fill_triangle(cx - 2, cy + 14, cx - 13, cy + 40, cx + 5, cy + 23,
                                 rgb565(255, 216, 73));
    }
    if (rainy) {
        for (int i = 0; i < 4; ++i) {
            int x = cx - 20 + i * 13;
            ESP_RETURN_ON_ERROR(lcd_draw_line(x, cy + 26, x - 5, cy + 39, 2, rain),
                                TAG, "draw rain failed");
        }
        return ESP_OK;
    }
    if (snowy) {
        for (int i = 0; i < 4; ++i) {
            ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 19 + i * 13, cy + 32 + (i % 2) * 5,
                                                2, snow),
                                TAG, "draw snow failed");
        }
    }
    if (foggy) {
        ESP_RETURN_ON_ERROR(lcd_draw_hline(cx - 28, cy + 29, 56, shade), TAG, "draw fog failed");
        return lcd_draw_hline(cx - 22, cy + 37, 44, shade);
    }
    return ESP_OK;
}

static esp_err_t draw_weather_mini_symbol(const char *condition, int cx, int cy, uint16_t accent)
{
    const uint16_t sun = rgb565(255, 203, 84);
    const uint16_t cloud = rgb565(204, 224, 238);
    const uint16_t rain = rgb565(72, 206, 255);
    const uint16_t snow = rgb565(238, 250, 255);
    bool thunder = condition != NULL &&
                   (strstr(condition, "THUNDER") != NULL || strstr(condition, "STORM") != NULL);
    bool rainy = condition != NULL &&
                 (strstr(condition, "RAIN") != NULL || strstr(condition, "SHOWER") != NULL);
    bool snowy = condition != NULL && strstr(condition, "SNOW") != NULL;
    bool cloudy = condition != NULL &&
                  (strstr(condition, "CLOUD") != NULL || strstr(condition, "OVERCAST") != NULL);
    bool foggy = condition != NULL &&
                 (strstr(condition, "FOG") != NULL || strstr(condition, "MIST") != NULL ||
                  strstr(condition, "HAZE") != NULL);

    if (!rainy && !snowy && !cloudy && !foggy && !thunder) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 3, sun), TAG, "draw mini sun failed");
        ESP_RETURN_ON_ERROR(lcd_draw_line(cx - 5, cy, cx - 7, cy, 1, sun), TAG, "draw mini ray failed");
        ESP_RETURN_ON_ERROR(lcd_draw_line(cx + 5, cy, cx + 7, cy, 1, sun), TAG, "draw mini ray failed");
        ESP_RETURN_ON_ERROR(lcd_draw_line(cx, cy - 5, cx, cy - 7, 1, sun), TAG, "draw mini ray failed");
        return lcd_draw_line(cx, cy + 5, cx, cy + 7, 1, sun);
    }

    if (cloudy || foggy || rainy || snowy || thunder) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 4, cy + 1, 3, cloud), TAG, "draw mini cloud failed");
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx + 1, cy - 1, 4, cloud), TAG, "draw mini cloud failed");
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx + 6, cy + 2, 3, cloud), TAG, "draw mini cloud failed");
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(cx - 8, cy + 2, 17, 4, 2, cloud),
                            TAG, "draw mini cloud failed");
    }
    if (thunder) {
        return lcd_fill_triangle(cx, cy + 5, cx - 4, cy + 8, cx + 2, cy + 7, rgb565(255, 216, 73));
    }
    if (rainy) {
        ESP_RETURN_ON_ERROR(lcd_draw_line(cx - 5, cy + 6, cx - 7, cy + 8, 1, rain),
                            TAG, "draw mini rain failed");
        return lcd_draw_line(cx + 3, cy + 6, cx + 1, cy + 8, 1, rain);
    }
    if (snowy) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 4, cy + 7, 1, snow), TAG, "draw mini snow failed");
        return lcd_fill_circle(cx + 4, cy + 8, 1, snow);
    }
    if (foggy) {
        ESP_RETURN_ON_ERROR(lcd_draw_hline(cx - 8, cy + 6, 17, accent), TAG, "draw mini fog failed");
        return lcd_draw_hline(cx - 6, cy + 8, 13, accent);
    }
    return ESP_OK;
}

static void weather_forecast_label(const char *date_text, const struct tm *tm,
                                   char *label, size_t label_len)
{
    (void)tm;
    if (label == NULL || label_len == 0) {
        return;
    }
    strlcpy(label, "--", label_len);
    if (date_text == NULL) {
        return;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    if (sscanf(date_text, "%d-%d-%d", &year, &month, &day) != 3) {
        return;
    }

    snprintf(label, label_len, "%02d/%02d", month, day);
}

static esp_err_t draw_weather_forecast_rows(const struct tm *tm, const weather_info_t *weather)
{
    const uint16_t line = rgb565(29, 53, 72);
    const uint16_t text = rgb565(220, 238, 248);
    const uint16_t muted = rgb565(133, 158, 181);
    const uint16_t accent = rgb565(94, 236, 218);
    const int row_x = CLOCK_PANEL_X + 16;
    const int row_w = CLOCK_PANEL_W - 32;
    const int row_y0 = CLOCK_PANEL_Y + 116;
    const int row_h = 12;

    ESP_RETURN_ON_ERROR(draw_text5(CLOCK_PANEL_X + 20, CLOCK_PANEL_Y + 106,
                                   "7D", 1, accent),
                        TAG, "draw forecast title failed");
    ESP_RETURN_ON_ERROR(lcd_draw_hline(CLOCK_PANEL_X + 42, CLOCK_PANEL_Y + 109,
                                       CLOCK_PANEL_W - 66, line),
                        TAG, "draw forecast title line failed");

    for (int i = 0; i < WEATHER_FORECAST_DAYS; ++i) {
        const bool valid = weather != NULL && i < weather->forecast_count && weather->forecast[i].valid;
        const char *condition = valid ? weather->forecast[i].condition : NULL;
        char label[12];
        char temp_buf[16];
        int y = row_y0 + i * row_h;

        if (valid) {
            weather_forecast_label(weather->forecast[i].date, tm, label, sizeof(label));
            snprintf(temp_buf, sizeof(temp_buf), "%d/%dC",
                     weather->forecast[i].min_c, weather->forecast[i].max_c);
        } else {
            strlcpy(label, "--", sizeof(label));
            strlcpy(temp_buf, "--/--C", sizeof(temp_buf));
        }

        if (i > 0) {
            ESP_RETURN_ON_ERROR(lcd_draw_hline(row_x, y - 2, row_w, line),
                                TAG, "draw forecast row line failed");
        }
        ESP_RETURN_ON_ERROR(draw_text_mixed_limited(row_x, y, label, 34, 1,
                                                    valid ? text : muted),
                            TAG, "draw forecast label failed");
        if (valid) {
            ESP_RETURN_ON_ERROR(draw_weather_mini_symbol(condition, row_x + 51, y + 3, accent),
                                TAG, "draw forecast mini failed");
        } else {
            ESP_RETURN_ON_ERROR(draw_text5(row_x + 45, y + 2, "--", 1, muted),
                                TAG, "draw forecast placeholder failed");
        }
        ESP_RETURN_ON_ERROR(draw_text_mixed_limited(row_x + 70, y, valid ? weather_condition_zh(condition) : "WAIT",
                                                    60, 1, valid ? text : muted),
                            TAG, "draw forecast condition failed");
        ESP_RETURN_ON_ERROR(draw_text5(CLOCK_PANEL_X + CLOCK_PANEL_W - text5_width(temp_buf, 1) - 18,
                                       y + 1, temp_buf, 1, valid ? accent : muted),
                            TAG, "draw forecast temp failed");
    }
    return ESP_OK;
}

static esp_err_t draw_weather_screen(const struct tm *tm)
{
    weather_info_t weather = {0};
    const char *condition_text = "天气";
    const char *city_text = CONFIG_CLOCK_WEATHER_CITY_LABEL;
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(145, 161, 196);
    const uint16_t accent = rgb565(95, 255, 221);
    const uint16_t band_top = rgb565(8, 28, 44);
    const uint16_t band_bottom = rgb565(10, 43, 50);
    char temp_buf[16];
    char hum_buf[16];
    char updated_buf[16];

    weather_get_snapshot(&weather);
    if (weather.valid) {
        snprintf(temp_buf, sizeof(temp_buf), "%dC", weather.temp_c);
        snprintf(hum_buf, sizeof(hum_buf), "%d%%", weather.humidity);
        condition_text = weather_condition_zh(weather.condition);
        if (weather.city[0] != '\0') {
            city_text = weather.city;
        }
        struct tm updated_tm = {0};
        localtime_r(&weather.updated_at, &updated_tm);
        snprintf(updated_buf, sizeof(updated_buf), "UP %02d:%02d",
                 updated_tm.tm_hour, updated_tm.tm_min);
    } else {
        strlcpy(temp_buf, "--C", sizeof(temp_buf));
        strlcpy(hum_buf, "--%", sizeof(hum_buf));
        strlcpy(updated_buf, "UP --:--", sizeof(updated_buf));
        condition_text = s_prov_active ? "配网" : "天气";
    }

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(5, 11, 25), rgb565(5, 25, 34)),
                        TAG, "draw weather bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("天气", tm), TAG, "draw weather header failed");

    ESP_RETURN_ON_ERROR(lcd_fill_vertical_blend_rect(CLOCK_PANEL_X + 1, CLOCK_PANEL_Y + 43,
                                                     CLOCK_PANEL_W - 2, 56,
                                                     band_top, band_bottom),
                        TAG, "draw weather band failed");
    ESP_RETURN_ON_ERROR(draw_weather_symbol(weather.valid ? weather.condition : NULL,
                                            CLOCK_PANEL_X + 52, CLOCK_PANEL_Y + 60),
                        TAG, "draw weather symbol failed");
    ESP_RETURN_ON_ERROR(draw_text5(CLOCK_PANEL_X + 105, CLOCK_PANEL_Y + 58, temp_buf, 4, text),
                        TAG, "draw weather temp failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(CLOCK_PANEL_X + 203, CLOCK_PANEL_Y + 61,
                                                city_text, 74, 1, muted),
                        TAG, "draw weather city failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(CLOCK_PANEL_X + 203, CLOCK_PANEL_Y + 78,
                                                condition_text, 74, 1, text),
                        TAG, "draw weather condition failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed(CLOCK_PANEL_X + 105, CLOCK_PANEL_Y + 94,
                                        "湿度", 1, muted),
                        TAG, "draw humidity label failed");
    ESP_RETURN_ON_ERROR(draw_text5(CLOCK_PANEL_X + 146, CLOCK_PANEL_Y + 96, hum_buf, 1, accent),
                        TAG, "draw humidity value failed");
    ESP_RETURN_ON_ERROR(draw_text5(CLOCK_PANEL_X + 203, CLOCK_PANEL_Y + 96,
                                   updated_buf, 1, muted),
                        TAG, "draw weather update failed");
    ESP_RETURN_ON_ERROR(draw_weather_forecast_rows(tm, &weather), TAG, "draw forecast failed");
    return lcd_canvas_flush();
}

static esp_err_t draw_current_view(const struct tm *tm, app_view_t view)
{
    switch (view) {
    case APP_VIEW_ANALOG_CLOCK:
        return draw_analog_clock_screen(tm);
    case APP_VIEW_CALENDAR:
        return draw_calendar_screen(tm);
    case APP_VIEW_WEATHER:
        return draw_weather_screen(tm);
    case APP_VIEW_DASHBOARD:
    default:
        return draw_clock_screen(tm);
    }
}

static void set_time_from_build(void)
{
    char month[4] = {0};
    int day = 1;
    int year = 2026;
    int hour = 0;
    int minute = 0;
    int second = 0;
    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";

    if (sscanf(__DATE__ " " __TIME__, "%3s %d %d %d:%d:%d",
               month, &day, &year, &hour, &minute, &second) != 6) {
        ESP_LOGW(TAG, "failed to parse build time, use default time");
        return;
    }

    const char *pos = strstr(months, month);
    int mon = pos != NULL ? (int)((pos - months) / 3) : 0;
    struct tm tm = {
        .tm_year = year - 1900,
        .tm_mon = mon,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = second,
    };
    time_t now = mktime(&tm);
    struct timeval tv = {
        .tv_sec = now,
        .tv_usec = 0,
    };
    ESP_ERROR_CHECK(settimeofday(&tv, NULL));
}

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase NVS failed");
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t wifi_credentials_load(wifi_credentials_t *creds)
{
    ESP_RETURN_ON_FALSE(creds != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid credentials output");
    memset(creds, 0, sizeof(*creds));

    nvs_handle_t nvs = 0;
    esp_err_t ret = nvs_open("wifi_cfg", NVS_READONLY, &nvs);
    if (ret == ESP_OK) {
        size_t ssid_len = sizeof(creds->ssid);
        size_t pass_len = sizeof(creds->password);
        esp_err_t ssid_ret = nvs_get_str(nvs, "ssid", creds->ssid, &ssid_len);
        esp_err_t pass_ret = nvs_get_str(nvs, "pass", creds->password, &pass_len);
        nvs_close(nvs);
        if (ssid_ret == ESP_OK && strlen(creds->ssid) > 0) {
            if (pass_ret != ESP_OK) {
                creds->password[0] = '\0';
            }
            creds->from_nvs = true;
            return ESP_OK;
        }
    }

    if (strlen(CONFIG_CLOCK_WIFI_SSID) == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    strlcpy(creds->ssid, CONFIG_CLOCK_WIFI_SSID, sizeof(creds->ssid));
    strlcpy(creds->password, CONFIG_CLOCK_WIFI_PASSWORD, sizeof(creds->password));
    creds->from_nvs = false;
    return ESP_OK;
}

static esp_err_t wifi_credentials_save(const char *ssid, const char *password)
{
    ESP_RETURN_ON_FALSE(ssid != NULL && strlen(ssid) > 0, ESP_ERR_INVALID_ARG, TAG, "empty SSID");
    ESP_RETURN_ON_FALSE(strlen(ssid) < WIFI_SSID_LEN, ESP_ERR_INVALID_ARG, TAG, "SSID too long");
    ESP_RETURN_ON_FALSE(password != NULL && strlen(password) < WIFI_PASSWORD_LEN,
                        ESP_ERR_INVALID_ARG, TAG, "password too long");

    nvs_handle_t nvs = 0;
    ESP_RETURN_ON_ERROR(nvs_open("wifi_cfg", NVS_READWRITE, &nvs), TAG, "open Wi-Fi NVS failed");
    esp_err_t ret = nvs_set_str(nvs, "ssid", ssid);
    if (ret == ESP_OK) {
        ret = nvs_set_str(nvs, "pass", password);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    ESP_RETURN_ON_ERROR(ret, TAG, "save Wi-Fi credentials failed");
    ESP_LOGI(TAG, "saved Wi-Fi credentials for SSID: %s", ssid);
    return ESP_OK;
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    ch = (char)tolower((unsigned char)ch);
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return -1;
}

static void url_decode_component(const char *src, char *dst, size_t dst_len)
{
    if (dst_len == 0) {
        return;
    }

    size_t out = 0;
    while (src != NULL && *src != '\0' && out + 1 < dst_len) {
        if (*src == '+') {
            dst[out++] = ' ';
            ++src;
        } else if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            int hi = hex_value(src[1]);
            int lo = hex_value(src[2]);
            dst[out++] = (char)((hi << 4) | lo);
            src += 3;
        } else {
            dst[out++] = *src++;
        }
    }
    dst[out] = '\0';
}

static bool form_get_value(const char *form, const char *key, char *out, size_t out_len)
{
    size_t key_len = strlen(key);
    const char *p = form;

    while (p != NULL && *p != '\0') {
        const char *next = strchr(p, '&');
        size_t pair_len = next != NULL ? (size_t)(next - p) : strlen(p);
        if (pair_len > key_len && strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            char encoded[PROV_FORM_MAX_LEN] = {0};
            size_t value_len = pair_len - key_len - 1;
            if (value_len >= sizeof(encoded)) {
                value_len = sizeof(encoded) - 1;
            }
            memcpy(encoded, p + key_len + 1, value_len);
            encoded[value_len] = '\0';
            url_decode_component(encoded, out, out_len);
            return true;
        }
        p = next != NULL ? next + 1 : NULL;
    }

    if (out_len > 0) {
        out[0] = '\0';
    }
    return false;
}

static esp_err_t provisioning_send_redirect(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", s_captive_portal_uri);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, "", 0);
}

static esp_err_t provisioning_root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_sendstr_chunk(req,
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>CYD Clock Wi-Fi</title>"
        "<style>"
        "body{margin:0;background:#101820;color:#eef7fa;font-family:-apple-system,BlinkMacSystemFont,"
        "'Segoe UI',sans-serif}main{max-width:420px;margin:0 auto;padding:28px 20px}"
        "h1{font-size:24px;margin:0 0 8px}p{color:#9fb7bf;line-height:1.5}"
        "label{display:block;margin:18px 0 8px;color:#c7dde3}input{box-sizing:border-box;width:100%;"
        "font-size:18px;padding:13px;border:1px solid #38515c;border-radius:8px;background:#071016;"
        "color:#eef7fa}button{width:100%;margin-top:22px;padding:14px;border:0;border-radius:8px;"
        "background:#ffd05c;color:#101820;font-size:18px;font-weight:700}"
        ".hint{font-size:13px;color:#86a2ab}</style></head><body><main>"
        "<h1>CYD Clock Wi-Fi</h1><p>输入家里的 Wi-Fi 信息，设备会保存并自动连接。</p>"
        "<form method='post' action='/save'>"
        "<label>Wi-Fi 名称</label><input name='ssid' maxlength='32' autocomplete='off' required>"
        "<label>Wi-Fi 密码</label><input name='password' maxlength='64' type='password' autocomplete='current-password'>"
        "<button type='submit'>保存并连接</button></form>"
        "<p class='hint'>如果手机没有自动弹出页面，请在浏览器打开 192.168.4.1。</p>"
        "</main></body></html>");
    return httpd_resp_sendstr_chunk(req, NULL);
}

static esp_err_t provisioning_save_handler(httpd_req_t *req)
{
    if (req->content_len == 0 || req->content_len >= PROV_FORM_MAX_LEN) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid form size");
        return ESP_FAIL;
    }

    char form[PROV_FORM_MAX_LEN] = {0};
    size_t received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, form + received, req->content_len - received);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "receive timeout");
            return ESP_FAIL;
        }
        received += (size_t)ret;
    }
    form[received] = '\0';

    char ssid[WIFI_SSID_LEN] = {0};
    char password[WIFI_PASSWORD_LEN] = {0};
    form_get_value(form, "ssid", ssid, sizeof(ssid));
    form_get_value(form, "password", password, sizeof(password));
    char *trimmed_ssid = trim_ascii(ssid);
    if (strlen(trimmed_ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty ssid");
        return ESP_FAIL;
    }

    esp_err_t ret = wifi_credentials_save(trimmed_ssid, password);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_sendstr(req,
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>已保存</title><style>body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
        "background:#101820;color:#eef7fa;padding:28px;line-height:1.6}</style></head>"
        "<body><h2>已保存</h2><p>设备正在连接 Wi-Fi。连接成功后，屏幕左上角会显示 WIFI OK。</p></body></html>");

    if (s_wifi_event_group != NULL) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_PROV_DONE_BIT);
    }
    return ESP_OK;
}

static esp_err_t provisioning_captive_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/") == 0) {
        return provisioning_root_handler(req);
    }
    return provisioning_send_redirect(req);
}

static void provisioning_dns_task(void *arg)
{
    (void)arg;
    uint8_t rx_buf[512];
    uint8_t tx_buf[512];

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "create DNS socket failed: errno=%d", errno);
        s_prov_dns_running = false;
        s_prov_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    struct timeval timeout = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in listen_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PROV_DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        ESP_LOGE(TAG, "bind DNS socket failed: errno=%d", errno);
        close(sock);
        s_prov_dns_running = false;
        s_prov_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    const uint32_t answer_ip = inet_addr(PROV_AP_IP_ADDR);
    while (s_prov_dns_running) {
        struct sockaddr_in source_addr = {0};
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buf, sizeof(rx_buf), 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 12) {
            continue;
        }

        int question_end = 12;
        while (question_end < len && rx_buf[question_end] != 0) {
            question_end += rx_buf[question_end] + 1;
        }
        question_end += 5;
        if (question_end > len || question_end + 16 > (int)sizeof(tx_buf)) {
            continue;
        }

        memcpy(tx_buf, rx_buf, (size_t)question_end);
        tx_buf[2] = 0x81;
        tx_buf[3] = 0x80;
        tx_buf[4] = 0x00;
        tx_buf[5] = 0x01;
        tx_buf[6] = 0x00;
        tx_buf[7] = 0x01;
        tx_buf[8] = 0x00;
        tx_buf[9] = 0x00;
        tx_buf[10] = 0x00;
        tx_buf[11] = 0x00;

        int pos = question_end;
        tx_buf[pos++] = 0xC0;
        tx_buf[pos++] = 0x0C;
        tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x01;
        tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x01;
        tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x3C;
        tx_buf[pos++] = 0x00;
        tx_buf[pos++] = 0x04;
        memcpy(&tx_buf[pos], &answer_ip, 4);
        pos += 4;

        (void)sendto(sock, tx_buf, (size_t)pos, 0, (struct sockaddr *)&source_addr, socklen);
    }

    close(sock);
    s_prov_dns_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t provisioning_start_http_server(void)
{
    if (s_prov_httpd != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = PROV_HTTP_PORT;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 6;

    ESP_RETURN_ON_ERROR(httpd_start(&s_prov_httpd, &config), TAG, "start provisioning HTTP server failed");

    const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = provisioning_root_handler,
    };
    const httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = provisioning_save_handler,
    };
    const httpd_uri_t captive_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = provisioning_captive_handler,
    };

    esp_err_t ret = httpd_register_uri_handler(s_prov_httpd, &root_uri);
    if (ret == ESP_OK) {
        ret = httpd_register_uri_handler(s_prov_httpd, &save_uri);
    }
    if (ret == ESP_OK) {
        ret = httpd_register_uri_handler(s_prov_httpd, &captive_uri);
    }
    if (ret != ESP_OK) {
        httpd_stop(s_prov_httpd);
        s_prov_httpd = NULL;
        ESP_LOGE(TAG, "register provisioning URI failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static void provisioning_stop_http_server(void)
{
    if (s_prov_httpd != NULL) {
        httpd_stop(s_prov_httpd);
        s_prov_httpd = NULL;
    }
}

static esp_err_t provisioning_start_dns(void)
{
    if (s_prov_dns_task != NULL) {
        return ESP_OK;
    }

    s_prov_dns_running = true;
    BaseType_t ok = xTaskCreate(provisioning_dns_task, "prov_dns", PROV_DNS_TASK_STACK_SIZE, NULL, 4,
                                &s_prov_dns_task);
    if (ok != pdPASS) {
        s_prov_dns_running = false;
        s_prov_dns_task = NULL;
        ESP_LOGE(TAG, "create DNS task failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void provisioning_stop_dns(void)
{
    s_prov_dns_running = false;
    for (int i = 0; i < 10 && s_prov_dns_task != NULL; ++i) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void provisioning_make_ap_ssid(void)
{
    uint8_t mac[6] = {0};
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (ret != ESP_OK) {
        strlcpy(s_prov_ap_ssid, CONFIG_CLOCK_PROV_AP_SSID_PREFIX, sizeof(s_prov_ap_ssid));
        return;
    }

    snprintf(s_prov_ap_ssid, sizeof(s_prov_ap_ssid), "%s-%02X%02X",
             CONFIG_CLOCK_PROV_AP_SSID_PREFIX, mac[4], mac[5]);
}

static void provisioning_configure_dhcp(void)
{
    if (s_ap_netif == NULL) {
        return;
    }

    esp_err_t ret = esp_netif_dhcps_stop(s_ap_netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG, "stop AP DHCP server failed: %s", esp_err_to_name(ret));
    }

    esp_netif_ip_info_t ip_info = {
        .ip = {.addr = ESP_IP4TOADDR(192, 168, 4, 1)},
        .gw = {.addr = ESP_IP4TOADDR(192, 168, 4, 1)},
        .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 0)},
    };
    ret = esp_netif_set_ip_info(s_ap_netif, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set AP IP info failed: %s", esp_err_to_name(ret));
    }

    esp_netif_dns_info_t dns_info = {
        .ip = ESP_IP4ADDR_INIT(192, 168, 4, 1),
    };
    ret = esp_netif_set_dns_info(s_ap_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set AP DNS info failed: %s", esp_err_to_name(ret));
    }

    uint8_t offer_dns = 1;
    ret = esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                 &offer_dns, sizeof(offer_dns));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set DHCP DNS option failed: %s", esp_err_to_name(ret));
    }

    ret = esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                                 (void *)s_captive_portal_uri, strlen(s_captive_portal_uri));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set DHCP captive portal option failed: %s", esp_err_to_name(ret));
    }

    ret = esp_netif_dhcps_start(s_ap_netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGW(TAG, "start AP DHCP server failed: %s", esp_err_to_name(ret));
    }
}

static esp_err_t wifi_start_if_needed(void)
{
    if (s_wifi_started) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start Wi-Fi failed");
    s_wifi_started = true;
    return ESP_OK;
}

static esp_err_t provisioning_start(void)
{
#if CONFIG_CLOCK_PROV_ENABLE
    provisioning_make_ap_ssid();
    provisioning_configure_dhcp();

    wifi_config_t ap_config = {0};
    strlcpy((char *)ap_config.ap.ssid, s_prov_ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(s_prov_ap_ssid);
    ap_config.ap.channel = CONFIG_CLOCK_PROV_AP_CHANNEL;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;

    if (strlen(CONFIG_CLOCK_PROV_AP_PASSWORD) >= 8) {
        strlcpy((char *)ap_config.ap.password, CONFIG_CLOCK_PROV_AP_PASSWORD, sizeof(ap_config.ap.password));
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    s_sta_retry_enabled = false;
    s_wifi_connected = false;
    s_wifi_configured = false;
    s_connected_ssid[0] = '\0';
    if (s_wifi_event_group != NULL) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "set AP mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_config), TAG, "set AP config failed");
    ESP_RETURN_ON_ERROR(wifi_start_if_needed(), TAG, "start Wi-Fi for AP failed");
    esp_err_t ret = provisioning_start_http_server();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = provisioning_start_dns();
    if (ret != ESP_OK) {
        provisioning_stop_http_server();
        return ret;
    }
    s_prov_active = true;

    ESP_LOGI(TAG, "provisioning portal started: SSID=%s URL=%s auth=%s",
             s_prov_ap_ssid, s_captive_portal_uri,
             ap_config.ap.authmode == WIFI_AUTH_OPEN ? "open" : "wpa2");
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static void provisioning_stop(void)
{
    if (!s_prov_active) {
        return;
    }

    provisioning_stop_dns();
    provisioning_stop_http_server();
    s_prov_active = false;
    ESP_LOGI(TAG, "provisioning portal stopped");
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_sta_retry_enabled) {
            esp_wifi_connect();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        s_connected_ssid[0] = '\0';
        if (s_wifi_event_group != NULL) {
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
        if (s_sta_retry_enabled && s_wifi_retry_count < WIFI_MAX_RETRY) {
            ++s_wifi_retry_count;
            ESP_LOGW(TAG, "Wi-Fi disconnected, retry %d/%d", s_wifi_retry_count, WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            s_sta_retry_enabled = false;
            s_wifi_retry_count = 0;
            if (s_wifi_event_group != NULL) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_retry_count = 0;
        s_wifi_connected = true;
        s_prov_active = false;
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

static esp_err_t wifi_stack_init(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_wifi_event_group != NULL, ESP_ERR_NO_MEM, TAG, "create Wi-Fi event group failed");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop init failed");
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(s_sta_netif != NULL && s_ap_netif != NULL, ESP_ERR_NO_MEM, TAG,
                        "create Wi-Fi netifs failed");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "set Wi-Fi storage failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler,
                                                            NULL, NULL),
                        TAG, "register Wi-Fi handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler,
                                                            NULL, NULL),
                        TAG, "register IP handler failed");
    return ESP_OK;
}

static esp_err_t wifi_connect_with_credentials(const wifi_credentials_t *creds, TickType_t timeout)
{
    ESP_RETURN_ON_FALSE(creds != NULL && strlen(creds->ssid) > 0, ESP_ERR_INVALID_ARG,
                        TAG, "invalid Wi-Fi credentials");

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, creds->ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, creds->password, sizeof(wifi_config.sta.password));
    if (strlen(creds->password) > 0) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    s_wifi_configured = true;
    s_wifi_connected = false;
    s_connected_ssid[0] = '\0';
    s_wifi_retry_count = 0;
    s_sta_retry_enabled = true;
    if (s_wifi_event_group != NULL) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set Wi-Fi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "set Wi-Fi config failed");
    bool was_started = s_wifi_started;
    ESP_RETURN_ON_ERROR(wifi_start_if_needed(), TAG, "start Wi-Fi failed");
    if (was_started) {
        (void)esp_wifi_disconnect();
        esp_err_t ret = esp_wifi_connect();
        if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN) {
            ESP_LOGW(TAG, "explicit Wi-Fi connect failed: %s", esp_err_to_name(ret));
        }
    }

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, timeout);
    if (bits & WIFI_CONNECTED_BIT) {
        strlcpy(s_connected_ssid, creds->ssid, sizeof(s_connected_ssid));
        ESP_LOGI(TAG, "Wi-Fi connected to %s", creds->ssid);
        return ESP_OK;
    }

    s_sta_retry_enabled = false;
    ESP_LOGW(TAG, "Wi-Fi connect failed or timeout");
    return ESP_FAIL;
}

static void sntp_time_sync_cb(struct timeval *tv)
{
    (void)tv;
    s_time_synced = true;
    ESP_LOGI(TAG, "SNTP time synchronized");
}

static esp_err_t sync_time_with_sntp(void)
{
    static bool sntp_initialized;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        4, ESP_SNTP_SERVER_LIST(CONFIG_CLOCK_NTP_SERVER, "ntp.aliyun.com", "ntp.tencent.com", "cn.pool.ntp.org"));
    if (!sntp_initialized) {
        config.sync_cb = sntp_time_sync_cb;
        ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&config), TAG, "SNTP init failed");
        sntp_initialized = true;
    }

    esp_err_t ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(CLOCK_SNTP_SYNC_TIMEOUT_MS));
    if (ret == ESP_OK) {
        s_time_synced = true;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "SNTP sync failed: %s", esp_err_to_name(ret));
    return ret;
}

static void start_weather_task_once(void)
{
#if CONFIG_CLOCK_WEATHER_ENABLE
    if (s_weather_task_started || s_wifi_event_group == NULL) {
        return;
    }

    BaseType_t task_ok = xTaskCreate(weather_task, "weather", WEATHER_TASK_STACK_SIZE, NULL, 4, NULL);
    if (task_ok == pdPASS) {
        s_weather_task_started = true;
    } else {
        ESP_LOGE(TAG, "create weather task failed");
    }
#endif
}

static void network_task(void *arg)
{
    (void)arg;

    esp_err_t ret = wifi_stack_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi stack init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    start_weather_task_once();

    for (;;) {
        wifi_credentials_t creds = {0};
        ret = wifi_credentials_load(&creds);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "try Wi-Fi SSID=%s source=%s", creds.ssid, creds.from_nvs ? "nvs" : "kconfig");
            provisioning_stop();
            ret = wifi_connect_with_credentials(&creds, pdMS_TO_TICKS(20000));
            if (ret == ESP_OK) {
                bool new_credentials = false;
                for (;;) {
                    if (!s_time_synced) {
                        (void)sync_time_with_sntp();
                    }
                    TickType_t wait_ticks = s_time_synced ? portMAX_DELAY : pdMS_TO_TICKS(60000);
                    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                                           WIFI_FAIL_BIT | WIFI_PROV_DONE_BIT,
                                                           pdTRUE, pdFALSE, wait_ticks);
                    if (bits & WIFI_PROV_DONE_BIT) {
                        new_credentials = true;
                        break;
                    }
                    if (bits & WIFI_FAIL_BIT) {
                        ESP_LOGW(TAG, "Wi-Fi lost after retries, enter provisioning");
                        break;
                    }
                }
                if (new_credentials) {
                    continue;
                }
            }
        } else {
            ESP_LOGI(TAG, "no Wi-Fi credentials found");
        }

        ret = provisioning_start();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "provisioning unavailable: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        xEventGroupWaitBits(s_wifi_event_group, WIFI_PROV_DONE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG, "new Wi-Fi credentials submitted");
    }
}

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int map_touch_axis(int raw, int raw_min, int raw_max, int screen_max)
{
    if (raw_max <= raw_min) {
        return 0;
    }
    int value = (raw - raw_min) * screen_max / (raw_max - raw_min);
    return clamp_int(value, 0, screen_max);
}

static bool touch_irq_is_pressed(void)
{
#if CONFIG_CLOCK_TOUCH_ENABLE
    if (!s_touch.initialized) {
        return false;
    }
    if (CONFIG_CLOCK_TOUCH_PIN_IRQ < 0) {
        return true;
    }
    int level = gpio_get_level(CONFIG_CLOCK_TOUCH_PIN_IRQ);
    return CONFIG_CLOCK_TOUCH_IRQ_ACTIVE_LOW ? (level == 0) : (level != 0);
#else
    return false;
#endif
}

static int touch_irq_level(void)
{
#if CONFIG_CLOCK_TOUCH_ENABLE
    if (!s_touch.initialized || CONFIG_CLOCK_TOUCH_PIN_IRQ < 0) {
        return -1;
    }
    return gpio_get_level(CONFIG_CLOCK_TOUCH_PIN_IRQ);
#else
    return -1;
#endif
}

static esp_err_t touch_read_axis(uint8_t command, uint16_t *value)
{
#if CONFIG_CLOCK_TOUCH_ENABLE
    ESP_RETURN_ON_FALSE(s_touch.spi != NULL && value != NULL, ESP_ERR_INVALID_STATE,
                        TAG, "touch SPI is unavailable");

    spi_transaction_t transaction = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 24,
        .rxlength = 24,
    };
    transaction.tx_data[0] = command;
    transaction.tx_data[1] = 0;
    transaction.tx_data[2] = 0;

    ESP_RETURN_ON_ERROR(spi_device_polling_transmit(s_touch.spi, &transaction),
                        TAG, "touch SPI transfer failed");
    *value = (uint16_t)((((uint16_t)transaction.rx_data[1] << 8) |
                         transaction.rx_data[2]) >> 3) & 0x0FFF;
    return ESP_OK;
#else
    (void)command;
    (void)value;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static uint16_t touch_pressure_from_z(uint16_t z1, uint16_t z2)
{
    int pressure = (int)z1 + 4095 - (int)z2;
    return (uint16_t)clamp_int(pressure, 0, 4095);
}

static bool touch_read_point(int *x, int *y, uint16_t *raw_x, uint16_t *raw_y,
                             uint16_t *pressure)
{
#if CONFIG_CLOCK_TOUCH_ENABLE
    bool irq_pressed = touch_irq_is_pressed();
    uint32_t sum_x = 0;
    uint32_t sum_y = 0;
    uint32_t sum_pressure = 0;
    int samples = 0;
    for (int i = 0; i < TOUCH_SAMPLE_COUNT; ++i) {
        uint16_t sample_x = 0;
        uint16_t sample_y = 0;
        uint16_t sample_z1 = 0;
        uint16_t sample_z2 = 0;
        if (touch_read_axis(0xB0, &sample_z1) != ESP_OK ||
            touch_read_axis(0xC0, &sample_z2) != ESP_OK ||
            touch_read_axis(0xD0, &sample_x) != ESP_OK ||
            touch_read_axis(0x90, &sample_y) != ESP_OK) {
            return false;
        }
        uint16_t sample_pressure = touch_pressure_from_z(sample_z1, sample_z2);
        if ((irq_pressed || sample_pressure >= CONFIG_CLOCK_TOUCH_PRESSURE_MIN) &&
            sample_x > 40 && sample_x < 4055 && sample_y > 40 && sample_y < 4055) {
            sum_x += sample_x;
            sum_y += sample_y;
            sum_pressure += sample_pressure;
            ++samples;
        }
    }

    if (samples == 0) {
        return false;
    }

    int rx = (int)(sum_x / (uint32_t)samples);
    int ry = (int)(sum_y / (uint32_t)samples);
    uint16_t avg_pressure = (uint16_t)(sum_pressure / (uint32_t)samples);
    if (!irq_pressed && avg_pressure < CONFIG_CLOCK_TOUCH_PRESSURE_MIN) {
        return false;
    }

    if (raw_x != NULL) {
        *raw_x = (uint16_t)rx;
    }
    if (raw_y != NULL) {
        *raw_y = (uint16_t)ry;
    }
    if (pressure != NULL) {
        *pressure = avg_pressure;
    }

    if (CONFIG_CLOCK_TOUCH_SWAP_XY) {
        int tmp = rx;
        rx = ry;
        ry = tmp;
    }

    int sx = map_touch_axis(rx, CONFIG_CLOCK_TOUCH_RAW_X_MIN,
                            CONFIG_CLOCK_TOUCH_RAW_X_MAX, LCD_H_RES - 1);
    int sy = map_touch_axis(ry, CONFIG_CLOCK_TOUCH_RAW_Y_MIN,
                            CONFIG_CLOCK_TOUCH_RAW_Y_MAX, LCD_V_RES - 1);
    if (CONFIG_CLOCK_TOUCH_MIRROR_X) {
        sx = LCD_H_RES - 1 - sx;
    }
    if (CONFIG_CLOCK_TOUCH_MIRROR_Y) {
        sy = LCD_V_RES - 1 - sy;
    }

    if (x != NULL) {
        *x = sx;
    }
    if (y != NULL) {
        *y = sy;
    }
    return true;
#else
    (void)x;
    (void)y;
    (void)raw_x;
    (void)raw_y;
    (void)pressure;
    return false;
#endif
}

static bool point_in_rect(int x, int y, int rect_x, int rect_y, int rect_w, int rect_h)
{
    return x >= rect_x && x < rect_x + rect_w && y >= rect_y && y < rect_y + rect_h;
}

static const char *view_name(app_view_t view)
{
    switch (view) {
    case APP_VIEW_ANALOG_CLOCK:
        return "time";
    case APP_VIEW_CALENDAR:
        return "calendar";
    case APP_VIEW_WEATHER:
        return "weather";
    case APP_VIEW_DASHBOARD:
    default:
        return "dashboard";
    }
}

static bool touch_poll_view_change(app_view_t *view)
{
#if CONFIG_CLOCK_TOUCH_ENABLE
    int x = 0;
    int y = 0;
    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    uint16_t pressure = 0;
    bool pressed = touch_read_point(&x, &y, &raw_x, &raw_y, &pressure);

    if (!pressed) {
        s_touch.was_pressed = false;
        return false;
    }
    if (s_touch.was_pressed) {
        return false;
    }
    s_touch.was_pressed = true;

    TickType_t now = xTaskGetTickCount();
    if ((now - s_touch.last_trigger_tick) < TOUCH_DEBOUNCE_TICKS) {
        return false;
    }
    s_touch.last_trigger_tick = now;

    app_view_t next_view = *view;
    if (*view != APP_VIEW_DASHBOARD) {
        next_view = APP_VIEW_DASHBOARD;
    } else if (point_in_rect(x, y, NAV_CARD_X0, NAV_CARD_Y, NAV_CARD_W, NAV_CARD_H)) {
        next_view = APP_VIEW_ANALOG_CLOCK;
    } else if (point_in_rect(x, y, NAV_CARD_X1, NAV_CARD_Y, NAV_CARD_W, NAV_CARD_H)) {
        next_view = APP_VIEW_CALENDAR;
    } else if (point_in_rect(x, y, NAV_CARD_X2, NAV_CARD_Y, NAV_CARD_W, NAV_CARD_H)) {
        next_view = APP_VIEW_WEATHER;
    } else if (*view == APP_VIEW_DASHBOARD &&
               point_in_rect(x, y, CLOCK_PANEL_X + 28, CLOCK_PANEL_Y + 48,
                             CLOCK_PANEL_W - 56, 60)) {
        next_view = APP_VIEW_ANALOG_CLOCK;
    } else if (*view == APP_VIEW_DASHBOARD &&
               point_in_rect(x, y, CLOCK_PANEL_X + 14, CLOCK_PANEL_Y + 112,
                             150, 42)) {
        next_view = APP_VIEW_CALENDAR;
    }

    ESP_LOGI(TAG, "touch raw=(%u,%u) pressure=%u irq=%d screen=(%d,%d) view=%s",
             raw_x, raw_y, pressure, touch_irq_level(), x, y, view_name(next_view));
    if (next_view != *view) {
        *view = next_view;
        return true;
    }
    return false;
#else
    (void)view;
    return false;
#endif
}

static esp_err_t touch_init(void)
{
#if CONFIG_CLOCK_TOUCH_ENABLE
    if (CONFIG_CLOCK_TOUCH_PIN_CS < 0) {
        ESP_LOGW(TAG, "touch disabled because CS GPIO is not configured");
        return ESP_OK;
    }

    spi_bus_config_t buscfg = {
        .sclk_io_num = CONFIG_CLOCK_TOUCH_PIN_SCLK,
        .mosi_io_num = CONFIG_CLOCK_TOUCH_PIN_MOSI,
        .miso_io_num = CONFIG_CLOCK_TOUCH_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 3,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_DISABLED),
                        TAG, "touch SPI bus init failed");

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = CONFIG_CLOCK_TOUCH_SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = CONFIG_CLOCK_TOUCH_PIN_CS,
        .queue_size = 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI3_HOST, &devcfg, &s_touch.spi),
                        TAG, "add touch SPI device failed");

    if (CONFIG_CLOCK_TOUCH_PIN_IRQ >= 0) {
        gpio_config_t irq_config = {
            .pin_bit_mask = 1ULL << CONFIG_CLOCK_TOUCH_PIN_IRQ,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&irq_config), TAG, "touch IRQ GPIO config failed");
    }

    s_touch.initialized = true;
    s_touch.was_pressed = false;
    s_touch.last_trigger_tick = 0;
    ESP_LOGI(TAG, "touch initialized: XPT2046 SCLK=%d MOSI=%d MISO=%d CS=%d IRQ=%d swap=%d mirror=(%d,%d)",
             CONFIG_CLOCK_TOUCH_PIN_SCLK, CONFIG_CLOCK_TOUCH_PIN_MOSI,
             CONFIG_CLOCK_TOUCH_PIN_MISO, CONFIG_CLOCK_TOUCH_PIN_CS,
             CONFIG_CLOCK_TOUCH_PIN_IRQ,
             CONFIG_CLOCK_TOUCH_SWAP_XY, CONFIG_CLOCK_TOUCH_MIRROR_X,
             CONFIG_CLOCK_TOUCH_MIRROR_Y);
    return ESP_OK;
#else
    ESP_LOGI(TAG, "touch disabled");
    return ESP_OK;
#endif
}

static esp_err_t lcd_init(void)
{
    s_lcd.done_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_lcd.done_sem != NULL, ESP_ERR_NO_MEM, TAG, "create LCD semaphore failed");

    s_lcd.draw_buf_pixels = LCD_H_RES * LCD_DRAW_LINES;
    s_lcd.draw_buf = heap_caps_malloc(s_lcd.draw_buf_pixels * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(s_lcd.draw_buf != NULL, ESP_ERR_NO_MEM, TAG, "allocate LCD DMA buffer failed");
    s_lcd.canvas_buf_pixels = CLOCK_PANEL_W * CLOCK_PANEL_H;
    s_lcd.canvas_buf = heap_caps_malloc(s_lcd.canvas_buf_pixels * sizeof(uint16_t),
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_lcd.canvas_buf != NULL, ESP_ERR_NO_MEM, TAG, "allocate LCD canvas buffer failed");

    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << CONFIG_CLOCK_LCD_PIN_BACKLIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bk_gpio_config), TAG, "backlight GPIO config failed");
    gpio_set_level(CONFIG_CLOCK_LCD_PIN_BACKLIGHT, CONFIG_CLOCK_LCD_BACKLIGHT_ACTIVE_HIGH ? 0 : 1);

    spi_bus_config_t buscfg = {
        .sclk_io_num = CONFIG_CLOCK_LCD_PIN_SCLK,
        .mosi_io_num = CONFIG_CLOCK_LCD_PIN_MOSI,
        .miso_io_num = CONFIG_CLOCK_LCD_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_DRAW_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG,
                        "SPI bus init failed");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = CONFIG_CLOCK_LCD_PIN_DC,
        .cs_gpio_num = CONFIG_CLOCK_LCD_PIN_CS,
        .pclk_hz = LCD_SPI_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 1,
        .on_color_trans_done = lcd_on_color_trans_done,
        .user_ctx = s_lcd.done_sem,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle),
                        TAG, "new LCD IO failed");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = CONFIG_CLOCK_LCD_PIN_RST,
        .rgb_ele_order = CONFIG_CLOCK_LCD_BGR_ORDER ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &s_lcd.panel), TAG,
                        "new ILI9341 panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_lcd.panel), TAG, "LCD reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_lcd.panel), TAG, "LCD init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_lcd.panel, true), TAG, "LCD swap XY failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_lcd.panel, CONFIG_CLOCK_LCD_MIRROR_X,
                                             CONFIG_CLOCK_LCD_MIRROR_Y), TAG, "LCD mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_lcd.panel, true), TAG, "LCD display on failed");

    gpio_set_level(CONFIG_CLOCK_LCD_PIN_BACKLIGHT, CONFIG_CLOCK_LCD_BACKLIGHT_ACTIVE_HIGH ? 1 : 0);
    ESP_LOGI(TAG, "LCD initialized: ILI9341 SPI 320x240 RGB565");
    return ESP_OK;
}

void app_main(void)
{
    setenv("TZ", CONFIG_CLOCK_TIMEZONE, 1);
    tzset();
    set_time_from_build();

    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(weather_state_init());
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(touch_init());
    ESP_ERROR_CHECK(draw_static_background());
    ESP_ERROR_CHECK(draw_text_mixed_centered(160, 112, "启动中", 1, rgb565(114, 238, 255)));

    BaseType_t network_ok = xTaskCreate(network_task, "network", 8192, NULL, 5, NULL);
    if (network_ok != pdPASS) {
        ESP_LOGE(TAG, "create network task failed");
    }

    int last_second = -1;
    app_view_t last_view = APP_VIEW_DASHBOARD;
    bool force_redraw = true;
    while (true) {
        time_t now = 0;
        struct tm tm = {0};
        time(&now);
        localtime_r(&now, &tm);

        if (touch_poll_view_change(&s_current_view)) {
            force_redraw = true;
        }

        if (s_current_view != last_view) {
            last_view = s_current_view;
            force_redraw = true;
        }

        if (force_redraw || tm.tm_sec != last_second) {
            last_second = tm.tm_sec;
            force_redraw = false;
            esp_err_t ret = draw_current_view(&tm, s_current_view);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "draw view failed: %s", esp_err_to_name(ret));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
