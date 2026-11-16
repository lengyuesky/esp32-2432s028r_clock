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
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_tls_crypto.h"
#include "esp_transport.h"
#include "esp_transport_ssl.h"
#include "esp_transport_tcp.h"
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
#define DEG_CHAR '^'
#define DEG_SIGN "^"
#define CLOCK_PANEL_X 12
#define CLOCK_PANEL_Y 20
#define CLOCK_PANEL_W 296
#define CLOCK_PANEL_H 200
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_PROV_DONE_BIT BIT2
#define WIFI_FORCE_PROV_BIT BIT3
#define WIFI_MAX_RETRY 8
#define WIFI_SAVED_CREDENTIALS_RETRY_MS 60000
#define WIFI_SSID_LEN 33
#define WIFI_PASSWORD_LEN 65
#define PROV_AP_IP_ADDR "192.168.4.1"
#define PROV_HTTP_PORT 80
#define PROV_DNS_PORT 53
#define PROV_DNS_TASK_STACK_SIZE 4096
#define PROV_FORM_MAX_LEN 1536
#define WEATHER_HTTP_BUFFER_SIZE 4096
#define WEATHER_TEXT_HTTP_BUFFER_SIZE 512
#define WEATHER_FORECAST_HTTP_BUFFER_SIZE 2048
#define WEATHER_HOURLY_HTTP_BUFFER_SIZE 3072
#define WEATHER_GEO_BUFFER_SIZE 1536
#define WEATHER_URL_BUFFER_SIZE 320
#define WEATHER_QUERY_CITY_LEN 64
#define WEATHER_IP_LEN 48
#define WEATHER_TEXT_LEN 32
#define WEATHER_CITY_LEN 32
#define WEATHER_FORECAST_DAYS 7
#define WEATHER_HOURLY_SLOTS 24
#define WEATHER_HOURLY_WINDOW_HOURS 24
#define WEATHER_HOURLY_STEP_HOURS 1
#define TODAY_HOURLY_PAGE_COUNT 2
#define TODAY_HOURLY_ITEMS_PER_PAGE 12
#define WEATHER_TASK_STACK_SIZE 16384
#define KOMARI_URL_LEN 192
#define KOMARI_HOST_LEN 96
#define KOMARI_PATH_LEN 96
#define KOMARI_TEXT_LEN 64
#define KOMARI_URL_BUFFER_SIZE 256
#define KOMARI_HTTP_BUFFER_SIZE 4096
#define KOMARI_WS_BUFFER_SIZE 4096
#define KOMARI_TASK_STACK_SIZE 8192
#define KOMARI_REFRESH_MS 60000
#define OTA_URL_LEN 256
#define OTA_TASK_STACK_SIZE 8192
#define SERIAL_COMMAND_TASK_STACK_SIZE 3072
#define OTA_HTTP_TIMEOUT_MS 15000
#define OTA_HTTP_REQUEST_SIZE 2048
#define OTA_HTTP_BUFFER_SIZE 2048
#define OTA_HTTP_TX_BUFFER_SIZE 1024
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
#define NAV_CARD_VALUE_SCALE 2
#define DASHBOARD_WEATHER_HIT_X (CLOCK_PANEL_X + CLOCK_PANEL_W - 82)
#define DASHBOARD_WEATHER_HIT_Y (CLOCK_PANEL_Y + 7)
#define DASHBOARD_WEATHER_HIT_W 72
#define DASHBOARD_WEATHER_HIT_H 47
#define HEALTH_STATUS_HIT_X (CLOCK_PANEL_X + 214)
#define HEALTH_STATUS_HIT_Y (CLOCK_PANEL_Y + 112)
#define HEALTH_STATUS_HIT_W 82
#define HEALTH_STATUS_HIT_H 42
#define WIFI_STATUS_HIT_X (CLOCK_PANEL_X + 8)
#define WIFI_STATUS_HIT_Y (CLOCK_PANEL_Y + 5)
#define WIFI_STATUS_HIT_W 134
#define WIFI_STATUS_HIT_H 34
#define WIFI_CONFIRM_BOX_X (CLOCK_PANEL_X + 40)
#define WIFI_CONFIRM_BOX_Y (CLOCK_PANEL_Y + 70)
#define WIFI_CONFIRM_BOX_W (CLOCK_PANEL_W - 80)
#define WIFI_CONFIRM_BOX_H 104
#define WIFI_CONFIRM_YES_X (WIFI_CONFIRM_BOX_X + 22)
#define WIFI_CONFIRM_YES_Y (WIFI_CONFIRM_BOX_Y + 66)
#define WIFI_CONFIRM_BUTTON_W 72
#define WIFI_CONFIRM_BUTTON_H 26
#define WIFI_CONFIRM_NO_X (WIFI_CONFIRM_BOX_X + WIFI_CONFIRM_BOX_W - WIFI_CONFIRM_BUTTON_W - 22)
#define WIFI_CONFIRM_NO_Y WIFI_CONFIRM_YES_Y
#define HEALTH_OTA_HIT_X (CLOCK_PANEL_X + CLOCK_PANEL_W - 82)
#define HEALTH_OTA_HIT_Y (CLOCK_PANEL_Y + CLOCK_PANEL_H - 31)
#define HEALTH_OTA_HIT_W 68
#define HEALTH_OTA_HIT_H 24
#define OTA_CONFIRM_BOX_X (CLOCK_PANEL_X + 36)
#define OTA_CONFIRM_BOX_Y (CLOCK_PANEL_Y + 58)
#define OTA_CONFIRM_BOX_W (CLOCK_PANEL_W - 72)
#define OTA_CONFIRM_BOX_H 128
#define OTA_CONFIRM_YES_X (OTA_CONFIRM_BOX_X + 24)
#define OTA_CONFIRM_YES_Y (OTA_CONFIRM_BOX_Y + 94)
#define OTA_CONFIRM_BUTTON_W 76
#define OTA_CONFIRM_BUTTON_H 26
#define OTA_CONFIRM_NO_X (OTA_CONFIRM_BOX_X + OTA_CONFIRM_BOX_W - OTA_CONFIRM_BUTTON_W - 24)
#define OTA_CONFIRM_NO_Y OTA_CONFIRM_YES_Y
#define MORE_TILE_X0 (CLOCK_PANEL_X + 14)
#define MORE_TILE_Y0 (CLOCK_PANEL_Y + 46)
#define MORE_TILE_W 130
#define MORE_TILE_H 68
#define MORE_TILE_GAP 8
#define MORE_TILE_COUNT 4
#define TIMER_RING_CX (CLOCK_PANEL_X + 84)
#define TIMER_RING_CY (CLOCK_PANEL_Y + 118)
#define TIMER_RING_RADIUS 68
#define TIMER_RING_THICKNESS 11
#define TIMER_SIDE_X (CLOCK_PANEL_X + 168)
#define TIMER_SIDE_W (CLOCK_PANEL_W - 168 - 14)
#define TIMER_SIDE_GAP 6
#define TIMER_SIDE_BUTTON_W ((TIMER_SIDE_W - TIMER_SIDE_GAP) / 2)
#define TIMER_PRESET_Y0 (CLOCK_PANEL_Y + 46)
#define TIMER_CONTROL_Y (CLOCK_PANEL_Y + 156)
#define TIMER_CONTROL_H 30
#define POMODORO_PRESET_H 48
#define COUNTDOWN_PRESET_H 30
#define COUNTDOWN_PRESET_GAP 5
#define TIMER_ALERT_WAKE_MS 60000
#define NIGHT_ROW_X (CLOCK_PANEL_X + 16)
#define NIGHT_ROW_RIGHT (CLOCK_PANEL_X + CLOCK_PANEL_W - 14)
#define NIGHT_TOGGLE_W 54
#define NIGHT_TOGGLE_H 26
#define NIGHT_TOGGLE_Y (CLOCK_PANEL_Y + 44)
#define NIGHT_ROW_Y0 (CLOCK_PANEL_Y + 78)
#define NIGHT_ROW_H 28
#define NIGHT_ROW_GAP 4
#define NIGHT_ROW_COUNT 3
#define NIGHT_STEP_BUTTON_W 48
#define NIGHT_MINUS_X (NIGHT_ROW_RIGHT - NIGHT_STEP_BUTTON_W * 2 - 6)
#define NIGHT_PLUS_X (NIGHT_ROW_RIGHT - NIGHT_STEP_BUTTON_W)
#define NIGHT_VALUE_CX (CLOCK_PANEL_X + 120)
#define NIGHT_TOUCH_WAKE_MS 15000
#define BACKLIGHT_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_RESOLUTION LEDC_TIMER_10_BIT
#define BACKLIGHT_LEDC_MAX_DUTY 1023
#define BACKLIGHT_LEDC_FREQ_HZ 5000

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

extern const uint8_t github_ota_roots_pem_start[] asm("_binary_github_ota_roots_pem_start");

typedef enum {
    APP_VIEW_DASHBOARD = 0,
    APP_VIEW_TODAY_WEATHER,
    APP_VIEW_WIFI_CONFIRM,
    APP_VIEW_OTA_CONFIRM,
    APP_VIEW_ANALOG_CLOCK,
    APP_VIEW_CALENDAR,
    APP_VIEW_WEATHER,
    APP_VIEW_KOMARI,
    APP_VIEW_HEALTH,
    APP_VIEW_MORE,
    APP_VIEW_POMODORO,
    APP_VIEW_COUNTDOWN,
    APP_VIEW_NIGHT,
} app_view_t;

typedef enum {
    OTA_STATUS_IDLE = 0,
    OTA_STATUS_RUNNING,
    OTA_STATUS_SUCCESS,
    OTA_STATUS_FAILED,
} ota_status_t;

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
    struct {
        bool valid;
        int temp_c;
        int humidity;
        int weather_code;
        char time[6];
        char condition[WEATHER_TEXT_LEN];
    } hourly[WEATHER_HOURLY_SLOTS];
    int hourly_count;
} weather_info_t;

typedef struct {
    char *data;
    int len;
    int cap;
    bool truncated;
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
    bool configured;
    bool valid;
    bool public_valid;
    bool nodes_valid;
    bool online_valid;
    int online_count;
    int node_count;
    bool private_site;
    bool record_enabled;
    time_t updated_at;
    char sitename[KOMARI_TEXT_LEN];
    char description[KOMARI_TEXT_LEN];
    char theme[24];
    char version[24];
    char base_url[KOMARI_URL_LEN];
} komari_info_t;

typedef struct {
    bool https;
    char host[KOMARI_HOST_LEN];
    int port;
    char path[KOMARI_PATH_LEN];
} komari_url_parts_t;

typedef struct {
    ota_status_t status;
    int progress_percent;
    int image_size;
    int image_read;
    esp_err_t last_error;
    time_t updated_at;
    char message[64];
    char override_url[OTA_URL_LEN];
} ota_info_t;

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
static SemaphoreHandle_t s_komari_lock;
static SemaphoreHandle_t s_ota_lock;
static SemaphoreHandle_t s_network_fetch_lock;
static weather_info_t s_weather;
static komari_info_t s_komari;
static ota_info_t s_ota;
static char s_komari_base_url[KOMARI_URL_LEN];
static char s_ota_override_url[OTA_URL_LEN];
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_prov_httpd;
static TaskHandle_t s_prov_dns_task;
static TaskHandle_t s_weather_task;
static TaskHandle_t s_komari_task;
static char s_prov_ap_ssid[WIFI_SSID_LEN];
static char s_connected_ssid[WIFI_SSID_LEN];
static int s_wifi_retry_count;
static bool s_wifi_started;
static bool s_weather_task_started;
static bool s_komari_task_started;
static volatile bool s_wifi_configured;
static volatile bool s_wifi_connected;
static volatile bool s_sta_retry_enabled;
static volatile bool s_prov_active;
static volatile bool s_prov_dns_running;
static volatile bool s_time_synced;
static volatile bool s_wifi_force_prov_pending;
static volatile bool s_ota_task_running;
static app_view_t s_current_view = APP_VIEW_DASHBOARD;
static int s_today_hourly_page;
static time_t s_boot_time;
static esp_reset_reason_t s_last_reset_reason = ESP_RST_UNKNOWN;

static void start_weather_task_once(void);
static void start_komari_task_once(void);

// ---------------------------------------------------------------------------
// 更多页小工具：番茄钟、倒计时和夜间模式的状态。
// 这些状态只在主循环任务中读写（触摸、绘制和计时轮询都在同一任务），不需要加锁。
// ---------------------------------------------------------------------------

typedef enum {
    POMODORO_PHASE_FOCUS = 0,
    POMODORO_PHASE_SHORT_BREAK,
    POMODORO_PHASE_LONG_BREAK,
} pomodoro_phase_t;

typedef struct {
    int64_t duration_us;
    int64_t remaining_us;
    int64_t deadline_us;
    bool running;
    bool finished;
} focus_timer_t;

typedef struct {
    const char *label;
    int minutes;
    pomodoro_phase_t phase;
} pomodoro_preset_t;

typedef struct {
    bool enabled;
    uint8_t start_hour;
    uint8_t end_hour;
    uint8_t dim_percent;
} night_mode_config_t;

static const pomodoro_preset_t s_pomodoro_presets[] = {
    {"专注", 25, POMODORO_PHASE_FOCUS},
    {"专注", 50, POMODORO_PHASE_FOCUS},
    {"短休", 5, POMODORO_PHASE_SHORT_BREAK},
    {"长休", 15, POMODORO_PHASE_LONG_BREAK},
};
#define POMODORO_PRESET_COUNT ((int)(sizeof(s_pomodoro_presets) / sizeof(s_pomodoro_presets[0])))
#define POMODORO_ROUNDS_PER_LONG_BREAK 4

static const int s_countdown_presets[] = {1, 3, 5, 10, 30, 60};
#define COUNTDOWN_PRESET_COUNT ((int)(sizeof(s_countdown_presets) / sizeof(s_countdown_presets[0])))

static const uint8_t s_night_dim_levels[] = {0, 2, 5, 10, 20, 40};
#define NIGHT_DIM_LEVEL_COUNT ((int)(sizeof(s_night_dim_levels) / sizeof(s_night_dim_levels[0])))

static focus_timer_t s_pomodoro_timer;
static focus_timer_t s_countdown_timer;
static int s_pomodoro_preset = 0;
static int s_pomodoro_completed;
static int s_countdown_minutes = 5;
static night_mode_config_t s_night = {
    .enabled = false,
    .start_hour = 22,
    .end_hour = 7,
    .dim_percent = 5,
};
static bool s_backlight_ready;
static int s_backlight_percent = -1;
static int64_t s_backlight_wake_until_us;

static int64_t focus_timer_remaining_us(const focus_timer_t *timer)
{
    if (!timer->running) {
        return timer->remaining_us > 0 ? timer->remaining_us : 0;
    }
    int64_t left = timer->deadline_us - esp_timer_get_time();
    return left > 0 ? left : 0;
}

static int focus_timer_remaining_seconds(const focus_timer_t *timer)
{
    return (int)((focus_timer_remaining_us(timer) + 999999) / 1000000);
}

static float focus_timer_fraction(const focus_timer_t *timer)
{
    if (timer->duration_us <= 0) {
        return 0.0f;
    }
    float fraction = (float)focus_timer_remaining_us(timer) / (float)timer->duration_us;
    if (fraction < 0.0f) {
        fraction = 0.0f;
    } else if (fraction > 1.0f) {
        fraction = 1.0f;
    }
    return fraction;
}

static bool focus_timer_is_paused(const focus_timer_t *timer)
{
    return !timer->running && !timer->finished && timer->remaining_us > 0 &&
           timer->remaining_us < timer->duration_us;
}

static void focus_timer_set(focus_timer_t *timer, int minutes)
{
    timer->duration_us = (int64_t)minutes * 60LL * 1000000LL;
    timer->remaining_us = timer->duration_us;
    timer->deadline_us = 0;
    timer->running = false;
    timer->finished = false;
}

static void focus_timer_start(focus_timer_t *timer)
{
    if (timer->duration_us <= 0 || timer->running) {
        return;
    }
    if (timer->remaining_us <= 0) {
        timer->remaining_us = timer->duration_us;
    }
    timer->deadline_us = esp_timer_get_time() + timer->remaining_us;
    timer->running = true;
    timer->finished = false;
}

static void focus_timer_pause(focus_timer_t *timer)
{
    if (!timer->running) {
        return;
    }
    timer->remaining_us = focus_timer_remaining_us(timer);
    timer->running = false;
}

static void focus_timer_toggle(focus_timer_t *timer)
{
    if (timer->running) {
        focus_timer_pause(timer);
    } else {
        focus_timer_start(timer);
    }
}

static void focus_timer_reset(focus_timer_t *timer)
{
    timer->remaining_us = timer->duration_us;
    timer->running = false;
    timer->finished = false;
}

static bool focus_timer_poll_finished(focus_timer_t *timer)
{
    if (!timer->running || esp_timer_get_time() < timer->deadline_us) {
        return false;
    }
    timer->running = false;
    timer->remaining_us = 0;
    timer->finished = true;
    return true;
}

static void format_timer_mmss(int seconds, char *out, size_t out_len)
{
    if (seconds < 0) {
        seconds = 0;
    }
    snprintf(out, out_len, "%02d:%02d", seconds / 60, seconds % 60);
}

static void pomodoro_apply_preset(int index, bool start)
{
    if (index < 0 || index >= POMODORO_PRESET_COUNT) {
        return;
    }
    s_pomodoro_preset = index;
    focus_timer_set(&s_pomodoro_timer, s_pomodoro_presets[index].minutes);
    if (start) {
        focus_timer_start(&s_pomodoro_timer);
    }
}

static pomodoro_phase_t pomodoro_phase(void)
{
    return s_pomodoro_presets[s_pomodoro_preset].phase;
}

static void pomodoro_on_finished(void)
{
    // 专注结束后自动准备休息（每 4 轮一次长休），休息结束后准备下一轮 25 分钟专注。
    int next = 0;
    if (pomodoro_phase() == POMODORO_PHASE_FOCUS) {
        ++s_pomodoro_completed;
        next = (s_pomodoro_completed % POMODORO_ROUNDS_PER_LONG_BREAK) == 0 ? 3 : 2;
    }
    pomodoro_apply_preset(next, false);
    s_pomodoro_timer.finished = true;
    ESP_LOGI(TAG, "pomodoro finished, completed=%d next=%s %dmin", s_pomodoro_completed,
             next == 0 ? "focus" : "break", s_pomodoro_presets[next].minutes);
}

static void countdown_apply_minutes(int minutes, bool start)
{
    s_countdown_minutes = minutes;
    focus_timer_set(&s_countdown_timer, minutes);
    if (start) {
        focus_timer_start(&s_countdown_timer);
    }
}

static void focus_tools_init(void)
{
    pomodoro_apply_preset(0, false);
    countdown_apply_minutes(s_countdown_minutes, false);
}

static bool night_hour_in_range(int hour, int start_hour, int end_hour)
{
    if (start_hour == end_hour) {
        return false;
    }
    if (start_hour < end_hour) {
        return hour >= start_hour && hour < end_hour;
    }
    return hour >= start_hour || hour < end_hour;
}

static bool night_mode_active(const struct tm *tm)
{
    return s_night.enabled && tm != NULL &&
           night_hour_in_range(tm->tm_hour, s_night.start_hour, s_night.end_hour);
}

static int night_dim_level_index(void)
{
    for (int i = 0; i < NIGHT_DIM_LEVEL_COUNT; ++i) {
        if (s_night_dim_levels[i] >= s_night.dim_percent) {
            return i;
        }
    }
    return NIGHT_DIM_LEVEL_COUNT - 1;
}

static esp_err_t night_config_load(void)
{
    nvs_handle_t nvs = 0;
    esp_err_t ret = nvs_open("night_cfg", NVS_READONLY, &nvs);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "open night NVS failed");

    uint8_t value = 0;
    if (nvs_get_u8(nvs, "enabled", &value) == ESP_OK) {
        s_night.enabled = value != 0;
    }
    if (nvs_get_u8(nvs, "start", &value) == ESP_OK && value < 24) {
        s_night.start_hour = value;
    }
    if (nvs_get_u8(nvs, "end", &value) == ESP_OK && value < 24) {
        s_night.end_hour = value;
    }
    if (nvs_get_u8(nvs, "dim", &value) == ESP_OK && value <= 100) {
        s_night.dim_percent = value;
    }
    nvs_close(nvs);
    ESP_LOGI(TAG, "night mode config: enabled=%d %02d:00-%02d:00 dim=%d%%",
             s_night.enabled, s_night.start_hour, s_night.end_hour, s_night.dim_percent);
    return ESP_OK;
}

static void night_config_save(void)
{
    nvs_handle_t nvs = 0;
    esp_err_t ret = nvs_open("night_cfg", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "open night NVS failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = nvs_set_u8(nvs, "enabled", s_night.enabled ? 1 : 0);
    if (ret == ESP_OK) {
        ret = nvs_set_u8(nvs, "start", s_night.start_hour);
    }
    if (ret == ESP_OK) {
        ret = nvs_set_u8(nvs, "end", s_night.end_hour);
    }
    if (ret == ESP_OK) {
        ret = nvs_set_u8(nvs, "dim", s_night.dim_percent);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "save night config failed: %s", esp_err_to_name(ret));
    }
}

static void backlight_gpio_fallback(bool on)
{
    int level = on ? 1 : 0;
    if (!CONFIG_CLOCK_LCD_BACKLIGHT_ACTIVE_HIGH) {
        level = !level;
    }
    gpio_set_level(CONFIG_CLOCK_LCD_PIN_BACKLIGHT, level);
}

static void backlight_apply_percent(int percent)
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    if (!s_backlight_ready) {
        // 没有 PWM 时只能整屏开/关：非零亮度一律视为全亮。
        percent = percent > 0 ? 100 : 0;
    }
    if (percent == s_backlight_percent) {
        return;
    }
    s_backlight_percent = percent;
    if (!s_backlight_ready) {
        backlight_gpio_fallback(percent > 0);
        return;
    }
    uint32_t duty = (uint32_t)((percent * BACKLIGHT_LEDC_MAX_DUTY + 50) / 100);
    esp_err_t ret = ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty);
    if (ret == ESP_OK) {
        ret = ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set backlight duty failed: %s", esp_err_to_name(ret));
    }
}

static esp_err_t backlight_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_RESOLUTION,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = BACKLIGHT_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "backlight LEDC timer config failed");

    ledc_channel_config_t channel_config = {
        .gpio_num = CONFIG_CLOCK_LCD_PIN_BACKLIGHT,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = BACKLIGHT_LEDC_MAX_DUTY,
        .hpoint = 0,
        .flags.output_invert = CONFIG_CLOCK_LCD_BACKLIGHT_ACTIVE_HIGH ? 0 : 1,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "backlight LEDC channel config failed");
    s_backlight_ready = true;
    s_backlight_percent = 100;
    ESP_LOGI(TAG, "backlight PWM ready on GPIO%d", CONFIG_CLOCK_LCD_PIN_BACKLIGHT);
    return ESP_OK;
}

static void backlight_wake(int duration_ms)
{
    int64_t until = esp_timer_get_time() + (int64_t)duration_ms * 1000LL;
    if (until > s_backlight_wake_until_us) {
        s_backlight_wake_until_us = until;
    }
    backlight_apply_percent(100);
}

static bool backlight_is_dimmed(void)
{
    return s_backlight_percent >= 0 && s_backlight_percent < 100;
}

static bool backlight_handle_touch_wake(void)
{
    // 夜间背光被调暗时，第一次点击只负责点亮屏幕，避免盲点误触进入其它页面。
    bool was_dimmed = backlight_is_dimmed();
    if (was_dimmed || s_backlight_wake_until_us > esp_timer_get_time()) {
        backlight_wake(NIGHT_TOUCH_WAKE_MS);
    }
    return was_dimmed;
}

static void backlight_update(const struct tm *tm)
{
    int target = 100;
    if (night_mode_active(tm) && esp_timer_get_time() >= s_backlight_wake_until_us) {
        target = s_night.dim_percent;
    }
    if (s_backlight_percent < 0 || target >= s_backlight_percent) {
        backlight_apply_percent(target);
        return;
    }
    // 调暗时逐步过渡，避免背光突然熄灭。
    int step = (s_backlight_percent - target + 3) / 4;
    if (step < 1) {
        step = 1;
    }
    backlight_apply_percent(s_backlight_percent - step);
}

static const char s_captive_portal_uri[] = "http://" PROV_AP_IP_ADDR "/";

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:
        return "poweron";
    case ESP_RST_EXT:
        return "external";
    case ESP_RST_SW:
        return "software";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_INT_WDT:
        return "interrupt-wdt";
    case ESP_RST_TASK_WDT:
        return "task-wdt";
    case ESP_RST_WDT:
        return "watchdog";
    case ESP_RST_DEEPSLEEP:
        return "deepsleep";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_SDIO:
        return "sdio";
    case ESP_RST_USB:
        return "usb";
    case ESP_RST_JTAG:
        return "jtag";
    case ESP_RST_EFUSE:
        return "efuse";
    case ESP_RST_PWR_GLITCH:
        return "power-glitch";
    case ESP_RST_CPU_LOCKUP:
        return "cpu-lockup";
    case ESP_RST_UNKNOWN:
    default:
        return "unknown";
    }
}

static const char *reset_reason_text_zh(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:
        return "上电";
    case ESP_RST_EXT:
        return "外部";
    case ESP_RST_SW:
        return "软件";
    case ESP_RST_PANIC:
        return "崩溃";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "看门狗";
    case ESP_RST_DEEPSLEEP:
        return "睡眠";
    case ESP_RST_BROWNOUT:
        return "欠压";
    case ESP_RST_SDIO:
    case ESP_RST_USB:
    case ESP_RST_JTAG:
    case ESP_RST_EFUSE:
        return "外设";
    case ESP_RST_PWR_GLITCH:
        return "电源";
    case ESP_RST_CPU_LOCKUP:
        return "锁死";
    case ESP_RST_UNKNOWN:
    default:
        return "未知";
    }
}

static void log_runtime_health(const char *scope)
{
    ESP_LOGI(TAG, "%s health: heap_free=%u heap_min=%u heap_largest=%u stack_free=%u",
             scope != NULL ? scope : "runtime",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

static esp_err_t network_fetch_lock_init(void)
{
    s_network_fetch_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_network_fetch_lock != NULL, ESP_ERR_NO_MEM, TAG,
                        "create network fetch lock failed");
    return ESP_OK;
}

static bool network_fetch_lock_take(const char *scope, TickType_t timeout)
{
    if (s_network_fetch_lock == NULL) {
        return true;
    }
    if (xSemaphoreTake(s_network_fetch_lock, timeout) == pdTRUE) {
        return true;
    }
    ESP_LOGW(TAG, "%s network fetch lock timeout", scope != NULL ? scope : "network");
    return false;
}

static void network_fetch_lock_give(void)
{
    if (s_network_fetch_lock != NULL) {
        xSemaphoreGive(s_network_fetch_lock);
    }
}

static void request_wifi_reprovision(void)
{
    if (s_prov_active) {
        ESP_LOGI(TAG, "manual provisioning request ignored: provisioning already active");
        return;
    }

    s_wifi_force_prov_pending = true;
    if (s_wifi_event_group != NULL) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FORCE_PROV_BIT);
    }
    ESP_LOGI(TAG, "manual provisioning requested from touch");
}

static bool wifi_reprovision_request_take(void)
{
    bool requested = s_wifi_force_prov_pending;
    if (s_wifi_event_group != NULL) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        if (bits & WIFI_FORCE_PROV_BIT) {
            requested = true;
            xEventGroupClearBits(s_wifi_event_group, WIFI_FORCE_PROV_BIT);
        }
    }
    if (requested) {
        s_wifi_force_prov_pending = false;
    }
    return requested;
}

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

static esp_err_t lcd_draw_arc(int cx, int cy, int radius,
                              int start_deg, int end_deg, int thickness,
                              uint16_t color)
{
    if (radius <= 0 || thickness <= 0 || start_deg == end_deg) {
        return ESP_OK;
    }

    int step = start_deg < end_deg ? 4 : -4;
    int deg = start_deg;
    for (;;) {
        float rad = (float)deg * (float)M_PI / 180.0f;
        int px = cx + (int)roundf(cosf(rad) * radius);
        int py = cy + (int)roundf(sinf(rad) * radius);
        ESP_RETURN_ON_ERROR(lcd_fill_circle(px, py, thickness / 2, color), TAG,
                            "draw arc failed");

        if ((step > 0 && deg >= end_deg) || (step < 0 && deg <= end_deg)) {
            break;
        }
        deg += step;
        if ((step > 0 && deg > end_deg) || (step < 0 && deg < end_deg)) {
            deg = end_deg;
        }
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
    // 半径超过短边一半时会让圆角越界 1 像素，这里统一收紧。
    if (radius * 2 > h - 1) {
        radius = (h - 1) / 2;
    }
    if (radius * 2 > w - 1) {
        radius = (w - 1) / 2;
    }
    if (radius < 0) {
        radius = 0;
    }
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
    {"更", {0x7FFE, 0x0180, 0x0180, 0x3FFC, 0x3184, 0x3FFC, 0x3184, 0x3184, 0x3FFC, 0x1100, 0x1900, 0x0F00, 0x0600, 0x1DE0, 0x703E, 0x0000}},
    {"新", {0x0800, 0x081E, 0x7F70, 0x2240, 0x2240, 0x1440, 0x7F7E, 0x084C, 0x084C, 0x7F4C, 0x184C, 0x1CCC, 0x2B8C, 0x488C, 0x090C, 0x090C}},
    {"正", {0x0000, 0x7FFE, 0x0080, 0x0080, 0x0080, 0x1080, 0x10FC, 0x1080, 0x1080, 0x1080, 0x1080, 0x1080, 0x1080, 0x7FFE, 0x0000, 0x0000}},
    {"在", {0x0200, 0x0200, 0x0200, 0x7FFE, 0x0400, 0x0820, 0x1820, 0x1020, 0x33FC, 0x5020, 0x1020, 0x1020, 0x1020, 0x1020, 0x17FE, 0x0000}},
    {"下", {0x7FFE, 0x0100, 0x0100, 0x0100, 0x01C0, 0x0170, 0x0118, 0x010C, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0000, 0x0000}},
    {"载", {0x0428, 0x3FA4, 0x0420, 0x7FFE, 0x0820, 0x0824, 0x7FA4, 0x1024, 0x2428, 0x3FB8, 0x0418, 0x0492, 0x7FB2, 0x046A, 0x04CE, 0x0000}},
    {"完", {0x0180, 0x0180, 0x7FFE, 0x4006, 0x4006, 0x0FF0, 0x0000, 0x0000, 0x7FFE, 0x0440, 0x0440, 0x0C40, 0x0842, 0x1842, 0x707E, 0x4000}},
    {"成", {0x004C, 0x0044, 0x3FFE, 0x2040, 0x2040, 0x2044, 0x3E4C, 0x2268, 0x2238, 0x2230, 0x2232, 0x2E72, 0x60D2, 0x419A, 0x410E, 0x0000}},
    {"失", {0x0180, 0x1980, 0x1180, 0x1FFC, 0x3180, 0x2180, 0x4180, 0x0100, 0x7FFE, 0x0180, 0x03C0, 0x0240, 0x0420, 0x1C18, 0x700E, 0x4002}},
    {"败", {0x0020, 0x7E20, 0x4240, 0x4A7E, 0x4A44, 0x4A84, 0x4AC4, 0x4AC8, 0x4A48, 0x4A28, 0x4A38, 0x1010, 0x1438, 0x366C, 0x62C6, 0x4082}},
    {"开", {0x7FFE, 0x0430, 0x0430, 0x0430, 0x0430, 0x0430, 0x7FFE, 0x0430, 0x0C30, 0x0830, 0x0830, 0x1830, 0x3030, 0x6030, 0x0030, 0x0000}},
    {"始", {0x1020, 0x1020, 0x1060, 0x1048, 0x7E44, 0x2686, 0x25FE, 0x2403, 0x6400, 0x64FE, 0x7886, 0x0886, 0x1C86, 0x3286, 0x60FE, 0x4086}},
    {"重", {0x0018, 0x3FE0, 0x0180, 0x7FFE, 0x0180, 0x1FFC, 0x118C, 0x1FFC, 0x118C, 0x118C, 0x1FFC, 0x0180, 0x3FFC, 0x0180, 0x7FFE, 0x0000}},
    {"试", {0x0010, 0x3014, 0x1812, 0x0810, 0x03FE, 0x0010, 0x7010, 0x1010, 0x13D0, 0x1090, 0x1098, 0x1098, 0x14AA, 0x1FCA, 0x320E, 0x0004}},
    {"返", {0x63FE, 0x3200, 0x1200, 0x0200, 0x03FC, 0x0284, 0x72C8, 0x1258, 0x1270, 0x1630, 0x14D8, 0x1586, 0x6800, 0x47FE, 0x0000, 0x0000}},
    {"回", {0x7FFE, 0x6006, 0x6006, 0x6006, 0x67E6, 0x6426, 0x6426, 0x6426, 0x6426, 0x67E6, 0x6006, 0x6006, 0x6006, 0x7FFE, 0x6006, 0x0000}},
    {"准", {0x0010, 0x0190, 0x6110, 0x3310, 0x13FE, 0x0620, 0x0E20, 0x03FE, 0x0220, 0x1220, 0x33FE, 0x2220, 0x6220, 0x4220, 0x03FE, 0x0200}},
    {"备", {0x0200, 0x0600, 0x0FF8, 0x1C30, 0x7660, 0x0180, 0x0E78, 0x700E, 0x1FF8, 0x1188, 0x1188, 0x1FF8, 0x1188, 0x1188, 0x1FF8, 0x1008}},
    {"等", {0x1040, 0x3040, 0x3FFE, 0x6990, 0x4180, 0x3FFC, 0x0180, 0x0180, 0x7FFE, 0x0010, 0x0010, 0x7FFE, 0x0810, 0x0410, 0x0210, 0x00F0}},
    {"待", {0x1020, 0x33FE, 0x6020, 0x4820, 0x1820, 0x17FE, 0x3008, 0x7008, 0x57FE, 0x1008, 0x1108, 0x1088, 0x10C8, 0x1008, 0x1038, 0x0000}},
    {"来", {0x0180, 0x0180, 0x3FFC, 0x0180, 0x1988, 0x0998, 0x0990, 0x0990, 0x7FFE, 0x03C0, 0x07E0, 0x0DB0, 0x1998, 0x318C, 0x4182, 0x0180}},
    {"源", {0x37FE, 0x1E30, 0x0620, 0x07FE, 0x4786, 0x37FE, 0x0786, 0x0586, 0x15FE, 0x1430, 0x14B4, 0x25B6, 0x2D32, 0x6B32, 0x0860, 0x0000}},
    {"设", {0x30F8, 0x1888, 0x0888, 0x0188, 0x010E, 0x7300, 0x1200, 0x13FC, 0x110C, 0x1188, 0x10D8, 0x1C70, 0x18F0, 0x339E, 0x0206, 0x0000}},
    {"状", {0x0868, 0x4864, 0x6866, 0x2860, 0x3FFE, 0x0860, 0x0860, 0x1870, 0x3870, 0x68D0, 0x4898, 0x0988, 0x0B0C, 0x0F06, 0x0A02, 0x0000}},
    {"态", {0x0180, 0x0180, 0x7FFE, 0x03C0, 0x0340, 0x0660, 0x0F38, 0x798E, 0x6102, 0x2988, 0x2CCC, 0x2C54, 0x6C16, 0x47F2, 0x0000, 0x0000}},
    {"地", {0x1020, 0x1120, 0x1126, 0x7D3E, 0x31F6, 0x13A6, 0x1726, 0x1126, 0x1126, 0x3D2C, 0x7120, 0x4102, 0x0102, 0x01FE, 0x0000, 0x0000}},
    {"址", {0x1030, 0x1030, 0x1030, 0x7D30, 0x1130, 0x113E, 0x1130, 0x1130, 0x1530, 0x1D30, 0x7130, 0x4130, 0x0130, 0x07FF, 0x0000, 0x0000}},
    {"信", {0x0BFC, 0x1800, 0x17FF, 0x3000, 0x7000, 0x73FC, 0xD000, 0x13FC, 0x1000, 0x13FC, 0x1204, 0x1204, 0x1204, 0x13FC, 0x1204, 0x0000}},
    {"号", {0x1008, 0x1008, 0x1008, 0x1FF8, 0x0000, 0x7FFE, 0x0C00, 0x0800, 0x1FF8, 0x1808, 0x0018, 0x0018, 0x0010, 0x01F0, 0x0000, 0x0000}},
    {"内", {0x0180, 0x7FFE, 0x6186, 0x6186, 0x6186, 0x6186, 0x63C6, 0x6666, 0x6E36, 0x7C1E, 0x6006, 0x6006, 0x6006, 0x603C, 0x0000, 0x0000}},
    {"存", {0x0200, 0x0600, 0x7FFE, 0x0400, 0x0C00, 0x19FC, 0x1818, 0x3030, 0x7020, 0x57FE, 0x1020, 0x1020, 0x1020, 0x1020, 0x11E0, 0x0000}},
    {"低", {0x083C, 0x1BF0, 0x1230, 0x3230, 0x7230, 0xF3FE, 0xF230, 0x3210, 0x3210, 0x3210, 0x33F8, 0x370B, 0x300F, 0x37F6, 0x1000, 0x0000}},
    {"值", {0x0860, 0x1FFE, 0x1040, 0x3040, 0x33FC, 0x7204, 0x73FC, 0x3204, 0x33FC, 0x3204, 0x3204, 0x33FC, 0x3204, 0x3FFE, 0x1000, 0x0000}},
    {"当", {0x318C, 0x118C, 0x1998, 0x0990, 0x0180, 0x3FFC, 0x000C, 0x0004, 0x0004, 0x1FFC, 0x000C, 0x0004, 0x000C, 0x3FFC, 0x0004, 0x0000}},
    {"前", {0x0C30, 0x7FFE, 0x0000, 0x0004, 0x3F24, 0x2324, 0x3F24, 0x2324, 0x2324, 0x3F24, 0x2324, 0x2304, 0x2304, 0x271C, 0x0000, 0x0000}},
    {"次", {0x2100, 0x3300, 0x1BFE, 0x0266, 0x0646, 0x0C64, 0x0464, 0x0860, 0x18E0, 0x30B0, 0x6190, 0x4318, 0x060C, 0x0C06, 0x0802, 0x0000}},
    {"监", {0x2460, 0x2460, 0x247E, 0x24C0, 0x24B0, 0x2598, 0x240C, 0x0000, 0x3FFC, 0x324C, 0x324C, 0x324C, 0x324C, 0x7FFE, 0x0000, 0x0000}},
    {"控", {0x13FE, 0x32D2, 0x7ED2, 0x1090, 0x1192, 0x131E, 0x7800, 0x73FE, 0x1020, 0x1020, 0x1020, 0x1020, 0x37FE, 0x7000, 0x0000, 0x0000}},
    {"版", {0x2CFE, 0x2C80, 0x2C80, 0x2C80, 0x3EFE, 0x20C6, 0x20E6, 0x61E4, 0x7DA4, 0x65BC, 0x6598, 0x4518, 0x453C, 0xC766, 0x0402, 0x0000}},
    {"本", {0x0180, 0x0180, 0x0180, 0x7FFE, 0x03C0, 0x03C0, 0x05E0, 0x0DB0, 0x1998, 0x318C, 0x6FF6, 0x4182, 0x0180, 0x0180, 0x0180, 0x0000}},
    {"关", {0x0C30, 0x0420, 0x0420, 0x1FF8, 0x0180, 0x0180, 0x0180, 0x1FF8, 0x0180, 0x0180, 0x03C0, 0x0660, 0x0E30, 0x1818, 0x1008, 0x0000}},
    {"闭", {0x1806, 0x0046, 0x6046, 0x6046, 0x6FFE, 0x60C6, 0x61C6, 0x6346, 0x6646, 0x7C46, 0x6046, 0x61C6, 0x6006, 0x601C, 0x0000, 0x0000}},
    {"空", {0x0180, 0x7FFE, 0x6666, 0x6646, 0x0440, 0x0C46, 0x387C, 0x6000, 0x0000, 0x1FFC, 0x0180, 0x0180, 0x0180, 0x7FFE, 0x0000, 0x0000}},
    {"闲", {0x1806, 0x0806, 0x6186, 0x6186, 0x6FFE, 0x61C6, 0x63C6, 0x67E6, 0x65B6, 0x6996, 0x6986, 0x6186, 0x6186, 0x601C, 0x0000, 0x0000}},
    {"秒", {0x3C30, 0x7830, 0x18B4, 0x18B4, 0x7FB6, 0x1932, 0x1B32, 0x3F30, 0x3C34, 0x786C, 0x580C, 0x1818, 0x1870, 0x19E0, 0x1900, 0x0000}},
    {"分", {0x0C30, 0x0818, 0x1808, 0x300C, 0x6006, 0x5FFA, 0x0308, 0x0208, 0x0218, 0x0618, 0x0418, 0x0C18, 0x3810, 0x70F0, 0x0000, 0x0000}},
    {"上", {0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x01FC, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x7FFE, 0x0000, 0x0000}},
    {"电", {0x0180, 0x0180, 0x3FFC, 0x218C, 0x218C, 0x3FFC, 0x218C, 0x218C, 0x218C, 0x3FFC, 0x2180, 0x0182, 0x0182, 0x00FE, 0x0000, 0x0000}},
    {"外", {0x0810, 0x1810, 0x1F90, 0x1190, 0x31D0, 0x7170, 0x7B30, 0x0E18, 0x061C, 0x0616, 0x0C12, 0x1810, 0x3010, 0x7010, 0x4010, 0x0000}},
    {"部", {0x0C7E, 0x7FE6, 0x2366, 0x3364, 0x326C, 0x1268, 0x7FEC, 0x0064, 0x3F62, 0x2162, 0x2162, 0x2166, 0x216E, 0x3F60, 0x2120, 0x0000}},
    {"软", {0x1860, 0x7F40, 0x107E, 0x10C2, 0x3CC2, 0x2C96, 0x6CB4, 0x7F30, 0x0C30, 0x0C30, 0x3F38, 0x7C68, 0x0CCC, 0x0D86, 0x0D02, 0x0000}},
    {"件", {0x0920, 0x1B20, 0x1360, 0x33FE, 0x3620, 0x7620, 0x5020, 0x1060, 0x17FE, 0x1020, 0x1020, 0x1020, 0x1020, 0x1020, 0x1020, 0x0000}},
    {"崩", {0x2184, 0x3FFC, 0x0000, 0x3E7C, 0x2244, 0x2244, 0x3E7C, 0x2244, 0x2244, 0x3E7C, 0x62C4, 0x62C4, 0x4284, 0x4E9C, 0x0000, 0x0000}},
    {"溃", {0x33FC, 0x1A64, 0x0264, 0x03FC, 0x4060, 0x77FE, 0x1000, 0x03FC, 0x0204, 0x1A64, 0x1264, 0x3264, 0x20D0, 0x638C, 0x0602, 0x0000}},
    {"看", {0x3FF0, 0x0300, 0x3FFC, 0x0200, 0x0600, 0x7FFE, 0x0C00, 0x1FFC, 0x380C, 0x6FFC, 0x480C, 0x0FFC, 0x080C, 0x0FFC, 0x0808, 0x0000}},
    {"门", {0x0180, 0x0180, 0x7FFE, 0x6186, 0x6186, 0x6186, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6004, 0x601C, 0x0000}},
    {"狗", {0x3980, 0x31FE, 0x7B02, 0x5A02, 0x0FF6, 0x1916, 0x1916, 0x3916, 0x6916, 0x49F6, 0x0906, 0x0804, 0x1804, 0x703C, 0x0000, 0x0000}},
    {"睡", {0x7FF8, 0x4C20, 0x4C20, 0x4FFE, 0x7D24, 0x4D24, 0x4FFF, 0x7D24, 0x4D24, 0x4D24, 0x4FFE, 0x7C20, 0x4020, 0x47FE, 0x0000, 0x0000}},
    {"眠", {0x4D06, 0x4D02, 0x4D02, 0x7DFE, 0x4D10, 0x4D10, 0x7DFE, 0x4D10, 0x4D18, 0x4D18, 0x7D08, 0x41B9, 0x03EF, 0x0206, 0x0000, 0x0000}},
    {"欠", {0x0800, 0x1800, 0x1FFE, 0x1186, 0x3184, 0x618C, 0x618C, 0x0188, 0x03C0, 0x0240, 0x0660, 0x0C30, 0x1818, 0x700E, 0x4002, 0x0000}},
    {"压", {0x2000, 0x20C0, 0x20C0, 0x20C0, 0x20C0, 0x2FFE, 0x20C0, 0x20D0, 0x20D8, 0x60CC, 0x60C0, 0x40C0, 0x5FFE, 0x4000, 0x0000, 0x0000}},
    {"锁", {0x31B6, 0x3EB4, 0x60B8, 0x4030, 0x3DFC, 0x1984, 0x11B4, 0x19B4, 0x7FB4, 0x11A4, 0x11A4, 0x1164, 0x1CD8, 0x198E, 0x0302, 0x0000}},
    {"死", {0x7FFE, 0x0840, 0x1840, 0x1F44, 0x314E, 0x2178, 0x7370, 0x5A60, 0x0E40, 0x0640, 0x0C42, 0x1842, 0x3062, 0x607E, 0x0000, 0x0000}},
    {"知", {0x3000, 0x207E, 0x7F46, 0x6846, 0x4846, 0x0846, 0x7FC6, 0x0846, 0x0846, 0x1C46, 0x1646, 0x3346, 0x317E, 0x6046, 0x4000, 0x0000}},
    {"自", {0x0300, 0x0600, 0x3FF8, 0x2008, 0x2008, 0x3FF8, 0x2008, 0x2008, 0x3FF8, 0x2008, 0x2008, 0x2008, 0x3FF8, 0x2008, 0x0000, 0x0000}},
    {"调", {0x43FC, 0x4244, 0x2244, 0x03F4, 0x0244, 0x8244, 0x2204, 0x22E4, 0x22B4, 0x22B4, 0x2AF4, 0x3204, 0x3404, 0x2C04, 0x1818, 0x0000}},
    {"暗", {0x0040, 0x7BFC, 0x4908, 0x4918, 0x4890, 0x4890, 0x7BFE, 0x4800, 0x49F8, 0x4908, 0x4908, 0x79F8, 0x0108, 0x01F8, 0x0108, 0x0000}},
    {"番", {0x3FA0, 0x0120, 0x0920, 0x0D40, 0x7FFC, 0x0780, 0x0D60, 0x313C, 0xFFF4, 0x1110, 0x1FF0, 0x1110, 0x1110, 0x1FF0, 0x1010, 0x0000}},
    {"茄", {0x0860, 0x0860, 0xFFFE, 0x0860, 0x1000, 0x1000, 0xFE44, 0x1244, 0x1244, 0x1244, 0x3244, 0x2244, 0x627C, 0x4244, 0x8C44, 0x0000}},
    {"钟", {0x3010, 0x3010, 0x2010, 0x3E10, 0x6092, 0x7E92, 0xD092, 0x1092, 0x7EFE, 0x1092, 0x1010, 0x1410, 0x1810, 0x1010, 0x0010, 0x0000}},
    {"倒", {0x0BF2, 0x0882, 0x110A, 0x134A, 0x33EA, 0x762A, 0x508A, 0x108A, 0x17EA, 0x108A, 0x1082, 0x17C2, 0x1402, 0x1006, 0x0000, 0x0000}},
    {"计", {0x4020, 0x2020, 0x1020, 0x0020, 0x0020, 0xE3FE, 0x2020, 0x2020, 0x2020, 0x2020, 0x2820, 0x3020, 0x3020, 0x2020, 0x0020, 0x0000}},
    {"夜", {0x0100, 0x7FFE, 0x0880, 0x0980, 0x19F8, 0x1308, 0x33C8, 0x7550, 0x5C90, 0x10E0, 0x1060, 0x10E0, 0x1198, 0x1E0E, 0x1000, 0x0000}},
    {"模", {0x1088, 0x1088, 0x13FE, 0x1088, 0x31FC, 0x3104, 0x39FC, 0x3D04, 0x71FC, 0x5020, 0x93FE, 0x1070, 0x10C8, 0x1186, 0x1200, 0x0000}},
    {"式", {0x0050, 0x0058, 0x0048, 0x7FFE, 0x0040, 0x0040, 0x7F40, 0x0840, 0x0820, 0x0820, 0x08B0, 0x0F12, 0xF00E, 0x0006, 0x0000, 0x0000}},
    {"专", {0x0100, 0x0300, 0x3FF8, 0x0200, 0x0200, 0xFFFE, 0x0400, 0x07F8, 0x0030, 0x0060, 0x06C0, 0x0380, 0x00C0, 0x0060, 0x0000, 0x0000}},
    {"注", {0x6080, 0x3040, 0x1000, 0x07FC, 0xC040, 0x6040, 0x0040, 0x1040, 0x17FC, 0x2040, 0x2040, 0x2040, 0x6040, 0x4FFE, 0x0000, 0x0000}},
    {"短", {0x1000, 0x11FE, 0x3E00, 0x28FC, 0x4884, 0x0884, 0x7E84, 0x08FC, 0x0880, 0x0C8C, 0x16C8, 0x1248, 0x2058, 0x61FF, 0x0000, 0x0000}},
    {"休", {0x1040, 0x1040, 0x1040, 0x2040, 0x27FC, 0x60E0, 0xE0E0, 0xA1E0, 0x2150, 0x2258, 0x264C, 0x2C46, 0x2840, 0x2040, 0x2040, 0x0000}},
    {"长", {0x0810, 0x0830, 0x0860, 0x08C0, 0x0B00, 0x0E00, 0xFFFE, 0x0980, 0x0880, 0x0840, 0x0820, 0x0818, 0x0B0E, 0x0C02, 0x0000, 0x0000}},
    {"暂", {0x0800, 0xFF7C, 0x1040, 0x2440, 0x3F4C, 0x0D44, 0x7C44, 0x0484, 0x0100, 0x1FF8, 0x1008, 0x1FF8, 0x1008, 0x1FF8, 0x1008, 0x0000}},
    {"停", {0x1080, 0x17FC, 0x1000, 0x23F8, 0x2208, 0x63F8, 0xE000, 0xAFFE, 0x2802, 0x23F8, 0x2040, 0x2040, 0x2040, 0x2040, 0x2180, 0x0000}},
    {"继", {0x0010, 0x1010, 0x1152, 0x2154, 0x6D74, 0x5910, 0x59FE, 0x1138, 0x213C, 0x6174, 0x7152, 0x0192, 0x0D10, 0x7110, 0x01FE, 0x0000}},
    {"续", {0x1020, 0x21FC, 0x2020, 0x4BFE, 0xF804, 0x10D0, 0x2030, 0x6110, 0x7090, 0x07FE, 0x0C38, 0x7064, 0x01C2, 0x0302, 0x0000, 0x0000}},
    {"置", {0x7FFC, 0x4444, 0x66CC, 0x7FFC, 0x0100, 0x0100, 0x1FF0, 0x1010, 0x1FF0, 0x1010, 0x1FF0, 0x1010, 0x1010, 0xFFFE, 0x0000, 0x0000}},
    {"结", {0x1010, 0x3010, 0x21FE, 0x4C10, 0x7810, 0x11FE, 0x2000, 0x64FC, 0x7084, 0x0084, 0x0C84, 0x7084, 0x00FC, 0x0084, 0x0000, 0x0000}},
    {"束", {0x0080, 0x0080, 0x7FFE, 0x0080, 0x1FF8, 0x1088, 0x1088, 0x1088, 0x1FF8, 0x01C0, 0x02C0, 0x0CB0, 0x708C, 0x4080, 0x0080, 0x0000}},
    {"亮", {0x0180, 0x7FFE, 0x0000, 0x0FF0, 0x0810, 0x0FF0, 0x0000, 0x7FFE, 0x4002, 0x47E2, 0x0420, 0x0420, 0x0822, 0x3822, 0x603E, 0x0000}},
    {"屏", {0x1FFC, 0x1004, 0x1004, 0x1FFC, 0x1310, 0x1190, 0x17FC, 0x1110, 0x1110, 0x1FFE, 0x1110, 0x3110, 0x2210, 0x6610, 0x0C10, 0x0000}},
    {"生", {0x0000, 0x0080, 0x0880, 0x1880, 0x1880, 0x1FFC, 0x3080, 0x2080, 0x6080, 0x5FFC, 0x0080, 0x0080, 0x0080, 0x0080, 0x7FFF, 0x0000}},
    {"效", {0x1040, 0x1840, 0x0840, 0x7FFE, 0x24C8, 0x26C8, 0x4748, 0xC748, 0x2C30, 0x3830, 0x0830, 0x1C38, 0x10CC, 0x6186, 0x4100, 0x0000}},
    {"轻", {0x3000, 0x21F8, 0xFC10, 0x6030, 0x5070, 0x5058, 0x508C, 0xFF02, 0x1200, 0x11FC, 0x3C20, 0x9020, 0x1020, 0x13FE, 0x1000, 0x0000}},
    {"触", {0x2010, 0x7810, 0x4810, 0xFCFC, 0xD494, 0x5494, 0x7C94, 0x54FC, 0x7C94, 0x5414, 0x5416, 0x5416, 0xD5F2, 0x9C00, 0x1000, 0x0000}},
    {"幕", {0x0620, 0x7FFF, 0x0420, 0x0FF8, 0x0808, 0x0FF8, 0x0808, 0x0300, 0x7FFE, 0x0498, 0x1FFC, 0x788B, 0x4888, 0x0890, 0x0080, 0x0000}},
    {"起", {0x08F8, 0x0808, 0x3E08, 0x0808, 0x08F8, 0x7E80, 0x0880, 0x2884, 0x2E84, 0x2884, 0x2878, 0x3800, 0x4E00, 0x43FC, 0x0000, 0x0000}},
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
    if (ch == '-') {
        return lcd_fill_rect(x + scale, y + 3 * scale, 3 * scale, scale, color);
    }
    if (ch == '/') {
        for (int i = 0; i < 5; ++i) {
            ESP_RETURN_ON_ERROR(lcd_fill_rect(x + (4 - i) * scale, y + (1 + i) * scale,
                                              scale, scale, color), TAG, "draw slash failed");
        }
        return ESP_OK;
    }
    if (ch == DEG_CHAR) {
        // 5x7 字体没有度数符号，用左上角的小方环代替。
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + scale, y, 3 * scale, scale, color), TAG, "draw degree failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + scale, y + 2 * scale, 3 * scale, scale, color), TAG,
                            "draw degree failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + scale, y + scale, scale, scale, color), TAG, "draw degree failed");
        return lcd_fill_rect(x + 3 * scale, y + scale, scale, scale, color);
    }
    if (ch == '+') {
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + scale, y + 3 * scale, 3 * scale, scale, color), TAG,
                            "draw plus failed");
        return lcd_fill_rect(x + 2 * scale, y + 2 * scale, scale, 3 * scale, color);
    }
    if (ch == '%') {
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x, y, 2 * scale, 2 * scale, color), TAG, "draw percent failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + 3 * scale, y + 5 * scale, 2 * scale, 2 * scale, color), TAG,
                            "draw percent failed");
        for (int i = 0; i < 5; ++i) {
            ESP_RETURN_ON_ERROR(lcd_fill_rect(x + (4 - i) * scale, y + (1 + i) * scale,
                                              scale, scale, color), TAG, "draw percent failed");
        }
        return ESP_OK;
    }
    if (ch == '?') {
        static const uint8_t question[5] = {0x02, 0x01, 0x51, 0x09, 0x06};
        for (int col = 0; col < 5; ++col) {
            for (int row = 0; row < 7; ++row) {
                if ((question[col] >> row) & 0x01) {
                    ESP_RETURN_ON_ERROR(lcd_fill_rect(x + col * scale, y + row * scale, scale, scale, color),
                                        TAG, "draw question failed");
                }
            }
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

#define SEG_DIGIT_W 40
#define SEG_DIGIT_H 58
#define SEG_THICK 8
#define SEG_DIGIT_GAP 10
#define SEG_COLON_W 10
#define SEG_COLON_GAP 10
#define DASHBOARD_TIME_Y (CLOCK_PANEL_Y + 50)

typedef struct {
    int y0;
    int h;
    uint16_t top;
    uint16_t bottom;
} seg_shade_t;

static uint16_t seg_shade_color(const seg_shade_t *shade, int y)
{
    int pos = y - shade->y0;
    if (pos < 0) {
        pos = 0;
    } else if (pos > shade->h - 1) {
        pos = shade->h - 1;
    }
    return mix_color(shade->top, shade->bottom, pos, shade->h > 1 ? shade->h - 1 : 1);
}

// 横向段：两端削成尖角，和竖向段在拐角处斜接，形成数码管风格的六边形笔画。
static esp_err_t draw_seg_horizontal(int x0, int x1, int y, int thick, const seg_shade_t *shade)
{
    for (int r = 0; r < thick; ++r) {
        int inset = abs(2 * r - (thick - 1)) / 2 + 1;
        int w = (x1 - x0) - inset * 2;
        if (w > 0) {
            ESP_RETURN_ON_ERROR(lcd_fill_rect(x0 + inset, y + r, w, 1, seg_shade_color(shade, y + r)),
                                TAG, "draw seg row failed");
        }
    }
    return ESP_OK;
}

static esp_err_t draw_seg_vertical(int x, int y0, int y1, int thick, const seg_shade_t *shade)
{
    const int half = thick / 2;
    for (int y = y0 + 1; y < y1 - 1; ++y) {
        int d = y - y0;
        if (y1 - 1 - y < d) {
            d = y1 - 1 - y;
        }
        int inset = half - d;
        if (inset < 0) {
            inset = 0;
        }
        int w = thick - inset * 2;
        if (w > 0) {
            ESP_RETURN_ON_ERROR(lcd_fill_rect(x + inset, y, w, 1, seg_shade_color(shade, y)),
                                TAG, "draw seg col failed");
        }
    }
    return ESP_OK;
}

static esp_err_t draw_seg_digit(int x, int y, int digit, bool unlit,
                                const seg_shade_t *shade)
{
    // bit0..bit6 依次为 a b c d e f g 段。
    static const uint8_t digit_segments[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
    };
    const int w = SEG_DIGIT_W;
    const int h = SEG_DIGIT_H;
    const int t = SEG_THICK;
    const int mid = y + h / 2;
    uint8_t mask = 0x7F;

    if (digit >= 0 && digit <= 9) {
        mask = digit_segments[digit];
    }
    if (unlit) {
        mask = (uint8_t)(0x7F & (uint8_t)~mask);
    }

    if (mask & 0x01) {
        ESP_RETURN_ON_ERROR(draw_seg_horizontal(x + t / 2, x + w - t / 2, y, t, shade), TAG, "seg a failed");
    }
    if (mask & 0x02) {
        ESP_RETURN_ON_ERROR(draw_seg_vertical(x + w - t, y + t / 2, mid, t, shade), TAG, "seg b failed");
    }
    if (mask & 0x04) {
        ESP_RETURN_ON_ERROR(draw_seg_vertical(x + w - t, mid, y + h - t / 2, t, shade), TAG, "seg c failed");
    }
    if (mask & 0x08) {
        ESP_RETURN_ON_ERROR(draw_seg_horizontal(x + t / 2, x + w - t / 2, y + h - t, t, shade), TAG,
                            "seg d failed");
    }
    if (mask & 0x10) {
        ESP_RETURN_ON_ERROR(draw_seg_vertical(x, mid, y + h - t / 2, t, shade), TAG, "seg e failed");
    }
    if (mask & 0x20) {
        ESP_RETURN_ON_ERROR(draw_seg_vertical(x, y + t / 2, mid, t, shade), TAG, "seg f failed");
    }
    if (mask & 0x40) {
        ESP_RETURN_ON_ERROR(draw_seg_horizontal(x + t / 2, x + w - t / 2, mid - t / 2, t, shade), TAG,
                            "seg g failed");
    }
    return ESP_OK;
}

static int dashboard_time_width(void)
{
    return SEG_DIGIT_W * 4 + SEG_DIGIT_GAP * 2 + SEG_COLON_GAP * 2 + SEG_COLON_W;
}

static int dashboard_time_x(void)
{
    return 160 - dashboard_time_width() / 2;
}

static esp_err_t draw_time_digits(const struct tm *tm)
{
    const int digits[4] = {tm->tm_hour / 10, tm->tm_hour % 10, tm->tm_min / 10, tm->tm_min % 10};
    const int y = DASHBOARD_TIME_Y;
    const int x0 = dashboard_time_x();
    const seg_shade_t lit = {y, SEG_DIGIT_H, rgb565(236, 250, 255), rgb565(112, 196, 255)};
    const seg_shade_t ghost = {y, SEG_DIGIT_H, rgb565(16, 26, 62), rgb565(18, 24, 66)};
    const seg_shade_t shadow = {y, SEG_DIGIT_H, rgb565(2, 4, 14), rgb565(2, 4, 14)};
    int xs[4];

    xs[0] = x0;
    xs[1] = xs[0] + SEG_DIGIT_W + SEG_DIGIT_GAP;
    xs[2] = xs[1] + SEG_DIGIT_W + SEG_COLON_GAP * 2 + SEG_COLON_W;
    xs[3] = xs[2] + SEG_DIGIT_W + SEG_DIGIT_GAP;

    for (int i = 0; i < 4; ++i) {
        ESP_RETURN_ON_ERROR(draw_seg_digit(xs[i], y, digits[i], true, &ghost), TAG, "draw ghost digit failed");
    }
    for (int i = 0; i < 4; ++i) {
        ESP_RETURN_ON_ERROR(draw_seg_digit(xs[i] + 2, y + 3, digits[i], false, &shadow),
                            TAG, "draw digit shadow failed");
    }
    for (int i = 0; i < 4; ++i) {
        ESP_RETURN_ON_ERROR(draw_seg_digit(xs[i], y, digits[i], false, &lit), TAG, "draw digit failed");
    }

    // 冒号随秒闪烁，偶数秒亮、奇数秒暗。
    const bool colon_on = (tm->tm_sec % 2) == 0;
    const int colon_x = xs[1] + SEG_DIGIT_W + SEG_COLON_GAP;
    const uint16_t colon = colon_on ? rgb565(120, 226, 255) : rgb565(30, 52, 92);
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(colon_x, y + SEG_DIGIT_H / 3 - 5, SEG_COLON_W, SEG_COLON_W, 3, colon),
                        TAG, "draw colon failed");
    return lcd_fill_round_rect(colon_x, y + (SEG_DIGIT_H * 2) / 3 - 4, SEG_COLON_W, SEG_COLON_W, 3, colon);
}

static esp_err_t draw_progress_dots(int second)
{
    // 时间下方的细进度条，表示当前分钟内已走过的秒数。
    const int x = dashboard_time_x();
    const int w = dashboard_time_width();
    const int y = DASHBOARD_TIME_Y + SEG_DIGIT_H + 8;
    const uint16_t track = rgb565(24, 34, 70);
    const uint16_t start = rgb565(76, 226, 255);
    const uint16_t end = rgb565(186, 110, 255);
    int fill = (w * (second + 1)) / 60;

    if (second < 0) {
        fill = 0;
    } else if (fill > w) {
        fill = w;
    }

    ESP_RETURN_ON_ERROR(lcd_fill_rect(x, y, w, 2, track), TAG, "draw seconds track failed");
    for (int i = 0; i < fill; i += 4) {
        int seg_w = fill - i < 4 ? fill - i : 4;
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + i, y, seg_w, 2, mix_color(start, end, i, w)),
                            TAG, "draw seconds fill failed");
    }
    if (fill > 0) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(x + fill - 1, y, 2, mix_color(start, end, fill, w)),
                            TAG, "draw seconds head failed");
    }
    return ESP_OK;
}

static esp_err_t draw_panel_frame(void)
{
    const uint16_t outer = rgb565(30, 44, 82);
    const uint16_t inner = rgb565(6, 12, 27);

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(CLOCK_PANEL_X, CLOCK_PANEL_Y,
                                            CLOCK_PANEL_W, CLOCK_PANEL_H, 10, outer),
                        TAG, "draw panel outer failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(CLOCK_PANEL_X + 1, CLOCK_PANEL_Y + 1,
                                            CLOCK_PANEL_W - 2, CLOCK_PANEL_H - 2, 9, inner),
                        TAG, "draw panel inner failed");
    return ESP_OK;
}

static int panel_row_inset(int row_y)
{
    // 面板内层圆角半径为 9，返回该行左右需要内缩的像素，避免渐变覆盖圆角外侧。
    const int radius = 9;
    const int top = CLOCK_PANEL_Y + 1;
    const int bottom = CLOCK_PANEL_Y + CLOCK_PANEL_H - 2;
    int d = row_y - top;
    if (bottom - row_y < d) {
        d = bottom - row_y;
    }
    if (d >= radius) {
        return 0;
    }
    int dy = radius - d;
    return radius - (int)sqrtf((float)(radius * radius - dy * dy));
}

static esp_err_t lcd_fill_panel_blend_rows(int y, int h, uint16_t top, uint16_t bottom)
{
    if (h <= 0) {
        return ESP_OK;
    }
    int den = h > 1 ? h - 1 : 1;
    for (int row = 0; row < h; ++row) {
        int inset = panel_row_inset(y + row);
        ESP_RETURN_ON_ERROR(lcd_fill_rect(CLOCK_PANEL_X + 1 + inset, y + row,
                                          CLOCK_PANEL_W - 2 - inset * 2, 1,
                                          mix_color(top, bottom, row, den)),
                            TAG, "fill panel rows failed");
    }
    return ESP_OK;
}

static esp_err_t draw_star_field(void)
{
    // 星点只放在时间数字之外的留白处，避免干扰读数。
    static const uint16_t stars[][2] = {
        {36, 62}, {58, 50}, {104, 58}, {214, 56}, {230, 66},
        {22, 98}, {298, 104}, {28, 130}, {294, 76}, {150, 64},
    };
    const uint16_t star = rgb565(196, 210, 255);
    const uint16_t star_dim = rgb565(76, 92, 150);

    for (size_t i = 0; i < sizeof(stars) / sizeof(stars[0]); ++i) {
        if ((i % 4) == 0) {
            ESP_RETURN_ON_ERROR(lcd_fill_rect(stars[i][0] - 1, stars[i][1], 3, 1, star_dim), TAG,
                                "draw star failed");
            ESP_RETURN_ON_ERROR(lcd_fill_rect(stars[i][0], stars[i][1] - 1, 1, 3, star_dim), TAG,
                                "draw star failed");
        }
        ESP_RETURN_ON_ERROR(lcd_fill_rect(stars[i][0], stars[i][1], 1, 1, (i % 3) == 0 ? star : star_dim),
                            TAG, "draw star failed");
    }
    return ESP_OK;
}

static esp_err_t draw_scenic_background(void)
{
    const uint16_t sky_top = rgb565(5, 8, 26);
    const uint16_t sky_mid = rgb565(14, 22, 62);
    const uint16_t sky_low = rgb565(24, 24, 74);
    const uint16_t floor_top = rgb565(10, 14, 38);
    const uint16_t floor_bottom = rgb565(5, 8, 22);
    const int hero_h = 150;

    ESP_RETURN_ON_ERROR(lcd_fill_panel_blend_rows(CLOCK_PANEL_Y + 1, 90, sky_top, sky_mid),
                        TAG, "draw sky failed");
    ESP_RETURN_ON_ERROR(lcd_fill_panel_blend_rows(CLOCK_PANEL_Y + 91, hero_h - 91, sky_mid, sky_low),
                        TAG, "draw sky low failed");
    ESP_RETURN_ON_ERROR(lcd_fill_panel_blend_rows(CLOCK_PANEL_Y + hero_h, CLOCK_PANEL_H - hero_h - 1,
                                                  floor_top, floor_bottom),
                        TAG, "draw floor failed");
    ESP_RETURN_ON_ERROR(draw_star_field(), TAG, "draw stars failed");

    // 地平线发光：中间亮、两端渐隐。
    const int line_y = CLOCK_PANEL_Y + hero_h;
    const int half = (CLOCK_PANEL_W - 2) / 2;
    const uint16_t glow = rgb565(132, 98, 255);
    for (int i = 0; i < half; i += 2) {
        uint16_t c = mix_color(glow, floor_top, i, half);
        ESP_RETURN_ON_ERROR(lcd_fill_rect(160 + i, line_y, 2, 1, c), TAG, "draw horizon failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(160 - i - 2, line_y, 2, 1, c), TAG, "draw horizon failed");
        uint16_t c2 = mix_color(rgb565(52, 40, 128), sky_low, i, half);
        ESP_RETURN_ON_ERROR(lcd_fill_rect(160 + i, line_y - 1, 2, 1, c2), TAG, "draw horizon failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(160 - i - 2, line_y - 1, 2, 1, c2), TAG, "draw horizon failed");
    }
    return ESP_OK;
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
    s_weather.hourly_count = 0;
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

static bool ascii_starts_with_ignore_case(const char *text, const char *prefix)
{
    if (text == NULL || prefix == NULL) {
        return false;
    }

    while (*prefix != '\0') {
        if (*text == '\0' || tolower((unsigned char)*text) != tolower((unsigned char)*prefix)) {
            return false;
        }
        ++text;
        ++prefix;
    }
    return true;
}

static bool komari_parse_base_url(const char *base_url, komari_url_parts_t *parts)
{
    if (base_url == NULL || parts == NULL) {
        return false;
    }

    memset(parts, 0, sizeof(*parts));
    const char *authority = NULL;
    if (strncmp(base_url, "https://", 8) == 0) {
        parts->https = true;
        parts->port = 443;
        authority = base_url + 8;
    } else if (strncmp(base_url, "http://", 7) == 0) {
        parts->https = false;
        parts->port = 80;
        authority = base_url + 7;
    } else {
        return false;
    }

    const char *path = strchr(authority, '/');
    size_t authority_len = path != NULL ? (size_t)(path - authority) : strlen(authority);
    if (authority_len == 0 || authority_len >= KOMARI_HOST_LEN) {
        return false;
    }

    char host_port[KOMARI_HOST_LEN] = {0};
    memcpy(host_port, authority, authority_len);
    host_port[authority_len] = '\0';

    char *port_text = strrchr(host_port, ':');
    if (port_text != NULL && port_text[1] != '\0') {
        bool numeric_port = true;
        for (char *p = port_text + 1; *p != '\0'; ++p) {
            if (!isdigit((unsigned char)*p)) {
                numeric_port = false;
                break;
            }
        }
        if (numeric_port) {
            long parsed_port = strtol(port_text + 1, NULL, 10);
            if (parsed_port <= 0 || parsed_port > 65535) {
                return false;
            }
            *port_text = '\0';
            parts->port = (int)parsed_port;
        }
    }

    if (host_port[0] == '\0') {
        return false;
    }
    for (const char *p = host_port; *p != '\0'; ++p) {
        if (isspace((unsigned char)*p)) {
            return false;
        }
    }
    strlcpy(parts->host, host_port, sizeof(parts->host));

    if (path != NULL && path[0] != '\0') {
        strlcpy(parts->path, path, sizeof(parts->path));
        char *query = strpbrk(parts->path, "?#");
        if (query != NULL) {
            *query = '\0';
        }
        size_t path_len = strlen(parts->path);
        while (path_len > 1 && parts->path[path_len - 1] == '/') {
            parts->path[--path_len] = '\0';
        }
        if (strcmp(parts->path, "/") == 0) {
            parts->path[0] = '\0';
        }
    }
    return true;
}

static bool komari_normalize_url(const char *input, char *output, size_t output_len)
{
    if (output == NULL || output_len == 0) {
        return false;
    }
    output[0] = '\0';

    if (input == NULL) {
        return true;
    }

    char scratch[KOMARI_URL_LEN] = {0};
    if (strlcpy(scratch, input, sizeof(scratch)) >= sizeof(scratch)) {
        return false;
    }
    char *trimmed = trim_ascii(scratch);
    if (trimmed[0] == '\0') {
        return true;
    }
    for (const char *p = trimmed; *p != '\0'; ++p) {
        if (isspace((unsigned char)*p)) {
            return false;
        }
    }

    char normalized[KOMARI_URL_LEN] = {0};
    int written = 0;
    if (ascii_starts_with_ignore_case(trimmed, "https://")) {
        written = snprintf(normalized, sizeof(normalized), "https://%s", trimmed + 8);
    } else if (ascii_starts_with_ignore_case(trimmed, "http://")) {
        written = snprintf(normalized, sizeof(normalized), "http://%s", trimmed + 7);
    } else if (strstr(trimmed, "://") == NULL) {
        written = snprintf(normalized, sizeof(normalized), "https://%s", trimmed);
    } else {
        return false;
    }
    if (written <= 0 || (size_t)written >= sizeof(normalized)) {
        return false;
    }

    char *query = strpbrk(normalized, "?#");
    if (query != NULL) {
        *query = '\0';
    }
    size_t len = strlen(normalized);
    while (len > 0 && normalized[len - 1] == '/') {
        normalized[--len] = '\0';
    }

    komari_url_parts_t parts = {0};
    if (!komari_parse_base_url(normalized, &parts)) {
        return false;
    }
    return strlcpy(output, normalized, output_len) < output_len;
}

static const char *ota_status_text(ota_status_t status)
{
    switch (status) {
    case OTA_STATUS_RUNNING:
        return "RUN";
    case OTA_STATUS_SUCCESS:
        return "OK";
    case OTA_STATUS_FAILED:
        return "ERR";
    case OTA_STATUS_IDLE:
    default:
        return "IDLE";
    }
}

static const char *ota_status_text_zh(ota_status_t status)
{
    switch (status) {
    case OTA_STATUS_RUNNING:
        return "下载";
    case OTA_STATUS_SUCCESS:
        return "完成";
    case OTA_STATUS_FAILED:
        return "失败";
    case OTA_STATUS_IDLE:
    default:
        return "空闲";
    }
}

static bool ota_normalize_url(const char *input, char *output, size_t output_len)
{
    if (output == NULL || output_len == 0) {
        return false;
    }
    output[0] = '\0';
    if (input == NULL) {
        return true;
    }

    char scratch[OTA_URL_LEN] = {0};
    if (strlcpy(scratch, input, sizeof(scratch)) >= sizeof(scratch)) {
        return false;
    }
    char *trimmed = trim_ascii(scratch);
    if (trimmed[0] == '\0') {
        return true;
    }
    for (const char *p = trimmed; *p != '\0'; ++p) {
        if (isspace((unsigned char)*p)) {
            return false;
        }
    }

    char normalized[OTA_URL_LEN] = {0};
    int written = 0;
    if (ascii_starts_with_ignore_case(trimmed, "https://")) {
        written = snprintf(normalized, sizeof(normalized), "https://%s", trimmed + 8);
    } else if (strstr(trimmed, "://") == NULL) {
        written = snprintf(normalized, sizeof(normalized), "https://%s", trimmed);
    } else {
        return false;
    }
    if (written <= 0 || (size_t)written >= sizeof(normalized)) {
        return false;
    }
    return strlcpy(output, normalized, output_len) < output_len;
}

static void ota_set_status(ota_status_t status, int progress_percent, int image_read,
                           int image_size, esp_err_t last_error, const char *message)
{
    if (s_ota_lock != NULL && xSemaphoreTake(s_ota_lock, pdMS_TO_TICKS(1000)) == pdTRUE) {
        s_ota.status = status;
        s_ota.progress_percent = progress_percent;
        s_ota.image_read = image_read;
        s_ota.image_size = image_size;
        s_ota.last_error = last_error;
        s_ota.updated_at = time(NULL);
        strlcpy(s_ota.message, message != NULL ? message : ota_status_text(status),
                sizeof(s_ota.message));
        xSemaphoreGive(s_ota_lock);
    }
}

static esp_err_t ota_state_init(void)
{
    s_ota_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_ota_lock != NULL, ESP_ERR_NO_MEM, TAG, "create OTA lock failed");
    memset(&s_ota, 0, sizeof(s_ota));
    s_ota.status = OTA_STATUS_IDLE;
    s_ota.progress_percent = -1;
    strlcpy(s_ota.message, "IDLE", sizeof(s_ota.message));
    s_ota_override_url[0] = '\0';
    return ESP_OK;
}

static void ota_get_snapshot(ota_info_t *out)
{
    if (out == NULL) {
        return;
    }
    if (s_ota_lock != NULL && xSemaphoreTake(s_ota_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
        *out = s_ota;
        xSemaphoreGive(s_ota_lock);
        return;
    }
    *out = s_ota;
}

static bool ota_get_effective_url(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return false;
    }

    if (s_ota_lock != NULL && xSemaphoreTake(s_ota_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (s_ota_override_url[0] != '\0') {
            strlcpy(out, s_ota_override_url, out_len);
        } else {
            strlcpy(out, CONFIG_CLOCK_OTA_DEFAULT_URL, out_len);
        }
        xSemaphoreGive(s_ota_lock);
    } else {
        strlcpy(out, s_ota_override_url[0] != '\0' ?
                s_ota_override_url : CONFIG_CLOCK_OTA_DEFAULT_URL, out_len);
    }
    return out[0] != '\0';
}

static void ota_apply_override_url(const char *url)
{
    if (s_ota_lock != NULL && xSemaphoreTake(s_ota_lock, pdMS_TO_TICKS(1000)) == pdTRUE) {
        strlcpy(s_ota_override_url, url != NULL ? url : "", sizeof(s_ota_override_url));
        strlcpy(s_ota.override_url, s_ota_override_url, sizeof(s_ota.override_url));
        xSemaphoreGive(s_ota_lock);
        return;
    }
    strlcpy(s_ota_override_url, url != NULL ? url : "", sizeof(s_ota_override_url));
    strlcpy(s_ota.override_url, s_ota_override_url, sizeof(s_ota.override_url));
}

static esp_err_t ota_config_load(void)
{
    char stored[OTA_URL_LEN] = {0};
    nvs_handle_t nvs = 0;
    esp_err_t ret = nvs_open("ota_cfg", NVS_READONLY, &nvs);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ota_apply_override_url("");
        ESP_LOGI(TAG, "OTA URL override is not configured");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "open OTA NVS failed");

    size_t len = sizeof(stored);
    ret = nvs_get_str(nvs, "url", stored, &len);
    nvs_close(nvs);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ota_apply_override_url("");
        ESP_LOGI(TAG, "OTA URL override is not configured");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "read OTA URL failed");

    char normalized[OTA_URL_LEN] = {0};
    if (!ota_normalize_url(stored, normalized, sizeof(normalized))) {
        ESP_LOGW(TAG, "ignore invalid saved OTA URL");
        ota_apply_override_url("");
        return ESP_OK;
    }
    ota_apply_override_url(normalized);
    ESP_LOGI(TAG, "OTA URL override loaded: %s",
             normalized[0] != '\0' ? normalized : "default");
    return ESP_OK;
}

static esp_err_t ota_config_save(const char *url)
{
    char normalized[OTA_URL_LEN] = {0};
    ESP_RETURN_ON_FALSE(ota_normalize_url(url, normalized, sizeof(normalized)),
                        ESP_ERR_INVALID_ARG, TAG, "invalid OTA URL");

    nvs_handle_t nvs = 0;
    ESP_RETURN_ON_ERROR(nvs_open("ota_cfg", NVS_READWRITE, &nvs), TAG, "open OTA NVS failed");
    esp_err_t ret = nvs_set_str(nvs, "url", normalized);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    ESP_RETURN_ON_ERROR(ret, TAG, "save OTA URL failed");

    ota_apply_override_url(normalized);
    ESP_LOGI(TAG, "saved OTA URL override: %s",
             normalized[0] != '\0' ? normalized : "default");
    return ESP_OK;
}

static void ota_mark_running_app_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running != NULL &&
        esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA image marked valid");
        } else {
            ESP_LOGW(TAG, "mark OTA image valid failed: %s", esp_err_to_name(ret));
        }
    }
}

static bool ota_stop_background_fetch_tasks(void)
{
    bool stopped = false;

#if CONFIG_CLOCK_WEATHER_ENABLE
    if (s_weather_task != NULL) {
        TaskHandle_t task = s_weather_task;
        s_weather_task = NULL;
        s_weather_task_started = false;
        vTaskDelete(task);
        stopped = true;
        ESP_LOGI(TAG, "weather task stopped for OTA");
    }
#endif

    if (s_komari_task != NULL) {
        TaskHandle_t task = s_komari_task;
        s_komari_task = NULL;
        s_komari_task_started = false;
        vTaskDelete(task);
        stopped = true;
        ESP_LOGI(TAG, "Komari task stopped for OTA");
    }

    if (stopped) {
        vTaskDelay(pdMS_TO_TICKS(100));
        log_runtime_health("ota-prep");
    }
    return stopped;
}

static void ota_restart_background_fetch_tasks(void)
{
    start_weather_task_once();
    start_komari_task_once();
}

static void ota_task(void *arg)
{
    char url[OTA_URL_LEN] = {0};
    if (arg != NULL) {
        strlcpy(url, (const char *)arg, sizeof(url));
        free(arg);
    }
    if (url[0] == '\0') {
        (void)ota_get_effective_url(url, sizeof(url));
    }

    ota_set_status(OTA_STATUS_RUNNING, 0, 0, 0, ESP_OK, "START");
    esp_err_t ret = ESP_OK;
    esp_https_ota_handle_t ota_handle = NULL;
    bool lock_taken = false;
    bool background_tasks_stopped = false;
    int last_read_len = 0;
    int last_image_size = 0;
    int last_progress = -1;
    int last_logged_progress = -1;
    if (!network_fetch_lock_take("ota", pdMS_TO_TICKS(120000))) {
        ret = ESP_ERR_TIMEOUT;
        goto fail;
    }
    lock_taken = true;
    background_tasks_stopped = ota_stop_background_fetch_tasks();

    bool use_github_ota_roots = strstr(url, "github.com/") != NULL ||
                                strstr(url, "githubusercontent.com/") != NULL;
    esp_http_client_config_t http_config = {
        .url = url,
        .cert_pem = use_github_ota_roots ? (const char *)github_ota_roots_pem_start : NULL,
        .timeout_ms = OTA_HTTP_TIMEOUT_MS,
        .buffer_size = OTA_HTTP_BUFFER_SIZE,
        .buffer_size_tx = OTA_HTTP_TX_BUFFER_SIZE,
        .keep_alive_enable = true,
        .crt_bundle_attach = use_github_ota_roots ? NULL : esp_crt_bundle_attach,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
        .bulk_flash_erase = false,
        .partial_http_download = true,
        .max_http_request_size = OTA_HTTP_REQUEST_SIZE,
    };

    ESP_LOGI(TAG, "OTA start: %s", url);
    ota_set_status(OTA_STATUS_RUNNING, 0, 0, 0, ESP_OK, "CONNECT");
    ret = esp_https_ota_begin(&ota_config, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OTA begin failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    while (true) {
        ret = esp_https_ota_perform(ota_handle);
        int read_len = esp_https_ota_get_image_len_read(ota_handle);
        int image_size = esp_https_ota_get_image_size(ota_handle);
        if (read_len > last_read_len) {
            last_read_len = read_len;
        }
        if (image_size > 0) {
            last_image_size = image_size;
        }
        int progress = image_size > 0 ? (read_len * 100) / image_size : -1;
        if (progress > 100) {
            progress = 100;
        }
        last_progress = progress;
        ota_set_status(OTA_STATUS_RUNNING, progress, read_len, image_size, ESP_OK, "RUN");
        if (progress >= 0 &&
            (last_logged_progress < 0 || progress >= last_logged_progress + 10 || progress == 100)) {
            ESP_LOGI(TAG, "OTA progress: %d%% (%d/%d bytes)", progress, read_len, image_size);
            last_logged_progress = progress;
        }
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OTA perform failed: %s", esp_err_to_name(ret));
        goto fail;
    }
    if (!esp_https_ota_is_complete_data_received(ota_handle)) {
        ret = ESP_ERR_INVALID_SIZE;
        ESP_LOGW(TAG, "OTA image incomplete");
        goto fail;
    }

    ret = esp_https_ota_finish(ota_handle);
    ota_handle = NULL;
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OTA finish failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ota_set_status(OTA_STATUS_SUCCESS, 100, last_read_len, last_image_size, ESP_OK, "REBOOT");
    ESP_LOGI(TAG, "OTA complete, restarting");
    if (lock_taken) {
        network_fetch_lock_give();
        lock_taken = false;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

fail:
    if (ota_handle != NULL) {
        esp_https_ota_abort(ota_handle);
    }
    if (lock_taken) {
        network_fetch_lock_give();
    }
    ota_set_status(OTA_STATUS_FAILED, last_progress, last_read_len, last_image_size,
                   ret, esp_err_to_name(ret));
    if (background_tasks_stopped) {
        ota_restart_background_fetch_tasks();
    }
    s_ota_task_running = false;
    vTaskDelete(NULL);
}

static esp_err_t ota_start_update(void)
{
    if (!s_wifi_connected) {
        ota_set_status(OTA_STATUS_FAILED, -1, 0, 0, ESP_ERR_INVALID_STATE, "NO WIFI");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ota_lock != NULL && xSemaphoreTake(s_ota_lock, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (s_ota_task_running || s_ota.status == OTA_STATUS_RUNNING) {
            xSemaphoreGive(s_ota_lock);
            return ESP_ERR_INVALID_STATE;
        }
        s_ota_task_running = true;
        xSemaphoreGive(s_ota_lock);
    } else if (s_ota_task_running) {
        return ESP_ERR_INVALID_STATE;
    }

    char *url = heap_caps_calloc(1, OTA_URL_LEN, MALLOC_CAP_8BIT);
    if (url == NULL) {
        s_ota_task_running = false;
        ota_set_status(OTA_STATUS_FAILED, -1, 0, 0, ESP_ERR_NO_MEM, "NO MEM");
        return ESP_ERR_NO_MEM;
    }
    if (!ota_get_effective_url(url, OTA_URL_LEN)) {
        free(url);
        s_ota_task_running = false;
        ota_set_status(OTA_STATUS_FAILED, -1, 0, 0, ESP_ERR_INVALID_ARG, "NO URL");
        return ESP_ERR_INVALID_ARG;
    }

    BaseType_t task_ok = xTaskCreate(ota_task, "ota", OTA_TASK_STACK_SIZE, url, 4, NULL);
    if (task_ok != pdPASS) {
        free(url);
        s_ota_task_running = false;
        ota_set_status(OTA_STATUS_FAILED, -1, 0, 0, ESP_ERR_NO_MEM, "TASK");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void serial_command_task(void *arg)
{
    (void)arg;
    char line[64];

    ESP_LOGI(TAG, "serial commands ready: ota, status");
    while (true) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        char *cmd = trim_ascii(line);
        for (char *p = cmd; *p != '\0'; ++p) {
            *p = (char)tolower((unsigned char)*p);
        }

        if (strcmp(cmd, "ota") == 0 || strcmp(cmd, "ota update") == 0) {
            esp_err_t ret = ota_start_update();
            ESP_LOGI(TAG, "serial ota command: %s", esp_err_to_name(ret));
        } else if (strcmp(cmd, "status") == 0) {
            ota_info_t ota = {0};
            ota_get_snapshot(&ota);
            ESP_LOGI(TAG,
                     "serial status: wifi=%d ota=%s progress=%d read=%d size=%d err=%s heap=%u",
                     s_wifi_connected,
                     ota_status_text(ota.status),
                     ota.progress_percent,
                     ota.image_read,
                     ota.image_size,
                     esp_err_to_name(ota.last_error),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
        } else if (cmd[0] != '\0') {
            ESP_LOGW(TAG, "unknown serial command: %s", cmd);
        }
    }
}

static void komari_reset_info_locked(const char *base_url)
{
    memset(&s_komari, 0, sizeof(s_komari));
    s_komari.configured = base_url != NULL && base_url[0] != '\0';
    s_komari.online_count = -1;
    s_komari.node_count = -1;
    strlcpy(s_komari.sitename, "--", sizeof(s_komari.sitename));
    strlcpy(s_komari.description, "--", sizeof(s_komari.description));
    strlcpy(s_komari.theme, "--", sizeof(s_komari.theme));
    strlcpy(s_komari.version, "--", sizeof(s_komari.version));
    if (base_url != NULL) {
        strlcpy(s_komari.base_url, base_url, sizeof(s_komari.base_url));
    }
}

static esp_err_t komari_state_init(void)
{
    s_komari_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_komari_lock != NULL, ESP_ERR_NO_MEM, TAG, "create Komari lock failed");
    s_komari_base_url[0] = '\0';
    komari_reset_info_locked("");
    return ESP_OK;
}

static void komari_get_snapshot(komari_info_t *out)
{
    if (out == NULL) {
        return;
    }

    if (s_komari_lock != NULL && xSemaphoreTake(s_komari_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
        *out = s_komari;
        xSemaphoreGive(s_komari_lock);
        return;
    }
    *out = s_komari;
}

static void komari_set_snapshot(const komari_info_t *info)
{
    if (info == NULL) {
        return;
    }

    if (s_komari_lock != NULL && xSemaphoreTake(s_komari_lock, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (strcmp(info->base_url, s_komari_base_url) == 0) {
            s_komari = *info;
            s_komari.configured = s_komari_base_url[0] != '\0';
        }
        xSemaphoreGive(s_komari_lock);
    }
}

static bool komari_configured(void)
{
    bool configured = false;
    if (s_komari_lock != NULL && xSemaphoreTake(s_komari_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
        configured = s_komari_base_url[0] != '\0';
        xSemaphoreGive(s_komari_lock);
        return configured;
    }
    return s_komari_base_url[0] != '\0';
}

static bool komari_get_base_url(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return false;
    }

    if (s_komari_lock != NULL && xSemaphoreTake(s_komari_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        strlcpy(out, s_komari_base_url, out_len);
        xSemaphoreGive(s_komari_lock);
    } else {
        strlcpy(out, s_komari_base_url, out_len);
    }
    return out[0] != '\0';
}

static void komari_apply_base_url(const char *base_url)
{
    if (s_komari_lock != NULL && xSemaphoreTake(s_komari_lock, pdMS_TO_TICKS(1000)) == pdTRUE) {
        strlcpy(s_komari_base_url, base_url != NULL ? base_url : "", sizeof(s_komari_base_url));
        komari_reset_info_locked(s_komari_base_url);
        xSemaphoreGive(s_komari_lock);
        return;
    }

    strlcpy(s_komari_base_url, base_url != NULL ? base_url : "", sizeof(s_komari_base_url));
    komari_reset_info_locked(s_komari_base_url);
}

static esp_err_t komari_config_load(void)
{
    char stored[KOMARI_URL_LEN] = {0};
    nvs_handle_t nvs = 0;
    esp_err_t ret = nvs_open("komari_cfg", NVS_READONLY, &nvs);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        komari_apply_base_url("");
        ESP_LOGI(TAG, "Komari URL is not configured");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "open Komari NVS failed");

    size_t len = sizeof(stored);
    ret = nvs_get_str(nvs, "url", stored, &len);
    nvs_close(nvs);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        komari_apply_base_url("");
        ESP_LOGI(TAG, "Komari URL is not configured");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "read Komari URL failed");

    char normalized[KOMARI_URL_LEN] = {0};
    if (!komari_normalize_url(stored, normalized, sizeof(normalized))) {
        ESP_LOGW(TAG, "ignore invalid saved Komari URL");
        komari_apply_base_url("");
        return ESP_OK;
    }
    komari_apply_base_url(normalized);
    ESP_LOGI(TAG, "Komari URL loaded: %s", normalized[0] != '\0' ? normalized : "not configured");
    return ESP_OK;
}

static esp_err_t komari_config_save(const char *base_url)
{
    char normalized[KOMARI_URL_LEN] = {0};
    ESP_RETURN_ON_FALSE(komari_normalize_url(base_url, normalized, sizeof(normalized)),
                        ESP_ERR_INVALID_ARG, TAG, "invalid Komari URL");

    nvs_handle_t nvs = 0;
    ESP_RETURN_ON_ERROR(nvs_open("komari_cfg", NVS_READWRITE, &nvs), TAG, "open Komari NVS failed");
    esp_err_t ret = nvs_set_str(nvs, "url", normalized);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    ESP_RETURN_ON_ERROR(ret, TAG, "save Komari URL failed");

    komari_apply_base_url(normalized);
    ESP_LOGI(TAG, "saved Komari URL: %s", normalized[0] != '\0' ? normalized : "not configured");
    return ESP_OK;
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
                           "https://wttr.in/%s?format=%%25l%%7C%%25t%%7C%%25h%%7C%%25C",
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
                           "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=10&language=zh&format=json",
                           encoded_city);
    return written > 0 && (size_t)written < url_len;
}

static bool weather_build_open_meteo_forecast_url(double latitude, double longitude,
                                                  char *url, size_t url_len)
{
    int written = snprintf(url, url_len,
                           "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
                           "&current=temperature_2m,relative_humidity_2m,weather_code"
                           "&daily=weather_code,temperature_2m_max,temperature_2m_min"
                           "&forecast_days=7&timezone=auto",
                           latitude, longitude);
    return written > 0 && (size_t)written < url_len;
}

static bool weather_build_open_meteo_hourly_url(double latitude, double longitude,
                                                char *url, size_t url_len)
{
    int written = snprintf(url, url_len,
                           "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
                           "&hourly=temperature_2m,relative_humidity_2m,weather_code"
                           "&forecast_days=2&timezone=auto",
                           latitude, longitude);
    return written > 0 && (size_t)written < url_len;
}

static bool weather_build_7timer_forecast_url(double latitude, double longitude,
                                              char *url, size_t url_len)
{
    int written = snprintf(url, url_len,
                           "https://www.7timer.info/bin/api.pl?lon=%.5f&lat=%.5f"
                           "&product=civillight&output=json",
                           longitude, latitude);
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

static const char *weather_condition_from_7timer(const char *code)
{
    if (code == NULL) {
        return "WEATHER";
    }
    if (ascii_equals_ignore_case(code, "ts")) {
        return "THUNDER";
    }
    if (strstr(code, "rain") != NULL || strstr(code, "shower") != NULL) {
        return "RAIN";
    }
    if (strstr(code, "snow") != NULL) {
        return "SNOW";
    }
    if (strstr(code, "fog") != NULL || strstr(code, "humid") != NULL) {
        return "FOG";
    }
    if (strstr(code, "cloud") != NULL) {
        return "CLOUD";
    }
    if (strstr(code, "clear") != NULL) {
        return "CLEAR";
    }
    return "WEATHER";
}

static bool weather_format_yyyymmdd(int date, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return false;
    }

    int year = date / 10000;
    int month = (date / 100) % 100;
    int day = date % 100;
    if (year < 2020 || month < 1 || month > 12 || day < 1 || day > 31) {
        out[0] = '\0';
        return false;
    }

    int written = snprintf(out, out_len, "%04d-%02d-%02d", year, month, day);
    return written > 0 && (size_t)written < out_len;
}

static void weather_copy_forecast(weather_info_t *dest, const weather_info_t *src)
{
    if (dest == NULL || src == NULL || src->forecast_count <= 0) {
        return;
    }

    int count = src->forecast_count;
    if (count > WEATHER_FORECAST_DAYS) {
        count = WEATHER_FORECAST_DAYS;
    }
    for (int i = 0; i < count; ++i) {
        dest->forecast[i] = src->forecast[i];
    }
    dest->forecast_count = count;
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

static bool weather_parse_hourly_time(const char *time_text, time_t *timestamp,
                                      int *hour, int *minute)
{
    if (time_text == NULL || timestamp == NULL || hour == NULL || minute == NULL) {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int parsed_hour = 0;
    int parsed_minute = 0;
    if (sscanf(time_text, "%d-%d-%dT%d:%d", &year, &month, &day,
               &parsed_hour, &parsed_minute) != 5) {
        return false;
    }

    if (year < 2020 ||
        month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        parsed_hour < 0 || parsed_hour > 23 ||
        parsed_minute < 0 || parsed_minute > 59) {
        return false;
    }

    struct tm parsed = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = parsed_hour,
        .tm_min = parsed_minute,
        .tm_sec = 0,
        .tm_isdst = -1,
    };
    time_t parsed_time = mktime(&parsed);
    if (parsed_time == (time_t)-1) {
        return false;
    }

    *timestamp = parsed_time;
    *hour = parsed_hour;
    *minute = parsed_minute;
    return true;
}

static bool weather_parse_open_meteo_hourly_response(const char *response, weather_info_t *out)
{
    if (response == NULL || out == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    if (root == NULL) {
        return false;
    }

    cJSON *hourly = cJSON_GetObjectItemCaseSensitive(root, "hourly");
    if (!cJSON_IsObject(hourly)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *times = cJSON_GetObjectItemCaseSensitive(hourly, "time");
    cJSON *temps = cJSON_GetObjectItemCaseSensitive(hourly, "temperature_2m");
    cJSON *humidity_values = cJSON_GetObjectItemCaseSensitive(hourly, "relative_humidity_2m");
    cJSON *codes = cJSON_GetObjectItemCaseSensitive(hourly, "weather_code");
    if (!cJSON_IsArray(times) || !cJSON_IsArray(temps) ||
        !cJSON_IsArray(humidity_values) || !cJSON_IsArray(codes)) {
        cJSON_Delete(root);
        return false;
    }

    time_t now = 0;
    time(&now);
    struct tm start_tm = {0};
    localtime_r(&now, &start_tm);
    start_tm.tm_min = 0;
    start_tm.tm_sec = 0;
    start_tm.tm_isdst = -1;
    time_t start_time = mktime(&start_tm);
    if (start_time == (time_t)-1) {
        cJSON_Delete(root);
        return false;
    }
    time_t end_time = start_time + WEATHER_HOURLY_WINDOW_HOURS * 60 * 60;

    out->hourly_count = 0;
    time_t last_time = start_time - WEATHER_HOURLY_STEP_HOURS * 60 * 60;
    int count = cJSON_GetArraySize(times);
    for (int i = 0; i < count && out->hourly_count < WEATHER_HOURLY_SLOTS; ++i) {
        const char *time_text = json_array_string(times, i);
        time_t hourly_time = 0;
        int hour = 0;
        int minute = 0;
        int temp_c = 0;
        int humidity = 0;
        int code = 0;
        if (!weather_parse_hourly_time(time_text, &hourly_time, &hour, &minute) ||
            hourly_time < start_time ||
            hourly_time >= end_time ||
            !json_array_int(temps, i, &temp_c) ||
            !json_array_int(humidity_values, i, &humidity) ||
            !json_array_int(codes, i, &code)) {
            continue;
        }

        if (out->hourly_count > 0 &&
            (hourly_time - last_time) < WEATHER_HOURLY_STEP_HOURS * 60 * 60) {
            continue;
        }

        int idx = out->hourly_count++;
        out->hourly[idx].valid = true;
        out->hourly[idx].temp_c = temp_c;
        out->hourly[idx].humidity = humidity;
        out->hourly[idx].weather_code = code;
        snprintf(out->hourly[idx].time, sizeof(out->hourly[idx].time),
                 "%02d:%02d", hour, minute);
        strlcpy(out->hourly[idx].condition, weather_condition_from_wmo(code),
                sizeof(out->hourly[idx].condition));
        last_time = hourly_time;
    }

    cJSON_Delete(root);
    return out->hourly_count > 0;
}

static bool weather_parse_7timer_forecast_response(const char *response, weather_info_t *out)
{
    if (response == NULL || out == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    if (root == NULL) {
        return false;
    }

    cJSON *dataseries = cJSON_GetObjectItemCaseSensitive(root, "dataseries");
    if (!cJSON_IsArray(dataseries)) {
        cJSON_Delete(root);
        return false;
    }

    out->forecast_count = 0;
    int count = cJSON_GetArraySize(dataseries);
    if (count > WEATHER_FORECAST_DAYS) {
        count = WEATHER_FORECAST_DAYS;
    }

    for (int i = 0; i < count; ++i) {
        cJSON *item = cJSON_GetArrayItem(dataseries, i);
        if (!cJSON_IsObject(item)) {
            continue;
        }

        int date = 0;
        if (!json_value_to_int(cJSON_GetObjectItemCaseSensitive(item, "date"), &date)) {
            continue;
        }

        cJSON *temp2m = cJSON_GetObjectItemCaseSensitive(item, "temp2m");
        int max_c = 0;
        int min_c = 0;
        if (!cJSON_IsObject(temp2m) ||
            !json_value_to_int(cJSON_GetObjectItemCaseSensitive(temp2m, "max"), &max_c) ||
            !json_value_to_int(cJSON_GetObjectItemCaseSensitive(temp2m, "min"), &min_c)) {
            continue;
        }

        const char *condition = json_string_item(item, "weather");
        weather_info_t *info = out;
        int idx = info->forecast_count++;
        info->forecast[idx].valid = true;
        info->forecast[idx].weather_code = -1;
        info->forecast[idx].min_c = min_c;
        info->forecast[idx].max_c = max_c;
        if (!weather_format_yyyymmdd(date, info->forecast[idx].date,
                                     sizeof(info->forecast[idx].date))) {
            info->forecast[idx].valid = false;
            --info->forecast_count;
            continue;
        }
        strlcpy(info->forecast[idx].condition, weather_condition_from_7timer(condition),
                sizeof(info->forecast[idx].condition));
    }

    cJSON_Delete(root);
    return out->forecast_count > 0;
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

static bool weather_parse_response(char *response, weather_info_t *out)
{
    if (response == NULL || out == NULL) {
        return false;
    }

    char *saveptr = NULL;
    char *first_text = strtok_r(response, "|", &saveptr);
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
        buffer->truncated = true;
        return ESP_OK;
    }

    int copy_len = evt->data_len;
    if (copy_len > remain) {
        copy_len = remain;
        buffer->truncated = true;
    }
    memcpy(buffer->data + buffer->len, evt->data, (size_t)copy_len);
    buffer->len += copy_len;
    buffer->data[buffer->len] = '\0';
    return ESP_OK;
}

static esp_err_t http_get_text_once(const char *url, char *response, size_t response_len,
                                    const char *label)
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
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    ESP_RETURN_ON_FALSE(client != NULL, ESP_ERR_NO_MEM, TAG, "init weather HTTP client failed");

    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (buffer.truncated && ret == ESP_OK) {
        ret = ESP_ERR_INVALID_SIZE;
    }

    if (ret != ESP_OK || status < 200 || status >= 300 || buffer.len <= 0) {
        ESP_LOGW(TAG, "%s HTTP failed: ret=%s status=%d len=%d truncated=%d url=%s",
                 label != NULL ? label : "http", esp_err_to_name(ret), status,
                 buffer.len, buffer.truncated, url);
        if (ret == ESP_OK) {
            ret = ESP_ERR_INVALID_RESPONSE;
        }
    }

    esp_http_client_cleanup(client);
    return ret;
}

static bool http_build_insecure_fallback_url(const char *url, char *fallback_url,
                                             size_t fallback_url_len)
{
    const char https_scheme[] = "https://";
    const char http_scheme[] = "http://";

    if (url == NULL || fallback_url == NULL ||
        strncmp(url, https_scheme, strlen(https_scheme)) != 0) {
        return false;
    }

    int written = snprintf(fallback_url, fallback_url_len, "%s%s",
                           http_scheme, url + strlen(https_scheme));
    return written > 0 && (size_t)written < fallback_url_len;
}

static esp_err_t http_get_text(const char *url, char *response, size_t response_len,
                               const char *label)
{
    esp_err_t ret = http_get_text_once(url, response, response_len, label);
    if (ret == ESP_OK || url == NULL || strncmp(url, "https://", 8) != 0) {
        return ret;
    }

    char fallback_url[WEATHER_URL_BUFFER_SIZE] = {0};
    if (!http_build_insecure_fallback_url(url, fallback_url, sizeof(fallback_url))) {
        return ret;
    }

    ESP_LOGW(TAG, "%s HTTPS failed, fallback to HTTP: %s",
             label != NULL ? label : "http", esp_err_to_name(ret));
    return http_get_text_once(fallback_url, response, response_len, label);
}

static void komari_set_default_text(komari_info_t *info)
{
    if (info == NULL) {
        return;
    }
    strlcpy(info->sitename, "--", sizeof(info->sitename));
    strlcpy(info->description, "--", sizeof(info->description));
    strlcpy(info->theme, "--", sizeof(info->theme));
    strlcpy(info->version, "--", sizeof(info->version));
}

static void komari_copy_text(const char *value, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    if (value == NULL || value[0] == '\0') {
        strlcpy(out, "--", out_len);
        return;
    }
    strlcpy(out, value, out_len);
}

static bool komari_json_bool(cJSON *object, const char *key, bool *value)
{
    if (object == NULL || key == NULL || value == NULL) {
        return false;
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsBool(item)) {
        *value = cJSON_IsTrue(item);
        return true;
    }
    if (cJSON_IsNumber(item)) {
        *value = item->valuedouble != 0.0;
        return true;
    }
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        *value = ascii_equals_ignore_case(item->valuestring, "true") ||
                 strcmp(item->valuestring, "1") == 0 ||
                 ascii_equals_ignore_case(item->valuestring, "yes");
        return true;
    }
    return false;
}

static bool komari_build_api_url(const char *base_url, const char *api_path,
                                 char *url, size_t url_len)
{
    if (base_url == NULL || base_url[0] == '\0' || api_path == NULL ||
        api_path[0] != '/' || url == NULL || url_len == 0) {
        return false;
    }

    int written = snprintf(url, url_len, "%s%s", base_url, api_path);
    return written > 0 && (size_t)written < url_len;
}

static bool komari_build_ws_path(const komari_url_parts_t *parts, char *path, size_t path_len)
{
    if (parts == NULL || path == NULL || path_len == 0) {
        return false;
    }

    int written = 0;
    if (parts->path[0] != '\0') {
        written = snprintf(path, path_len, "%s/api/clients", parts->path);
    } else {
        written = snprintf(path, path_len, "/api/clients");
    }
    return written > 0 && (size_t)written < path_len;
}

static bool komari_parse_public_response(const char *response, komari_info_t *info)
{
    if (response == NULL || info == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    if (root == NULL) {
        return false;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(data)) {
        data = root;
    }
    if (!cJSON_IsObject(data)) {
        cJSON_Delete(root);
        return false;
    }

    komari_copy_text(json_first_string(data, root, "sitename", "site_name", "name"),
                     info->sitename, sizeof(info->sitename));
    komari_copy_text(json_first_string(data, root, "description", "desc", NULL),
                     info->description, sizeof(info->description));
    komari_copy_text(json_first_string(data, root, "theme", NULL, NULL),
                     info->theme, sizeof(info->theme));
    komari_copy_text(json_first_string(data, root, "version", "komari_version", NULL),
                     info->version, sizeof(info->version));
    (void)komari_json_bool(data, "private_site", &info->private_site);
    (void)komari_json_bool(data, "record_enabled", &info->record_enabled);

    info->valid = true;
    info->public_valid = true;
    time(&info->updated_at);
    cJSON_Delete(root);
    return true;
}

static bool komari_parse_nodes_response(const char *response, komari_info_t *info)
{
    if (response == NULL || info == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(response);
    if (root == NULL) {
        return false;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsArray(data)) {
        data = root;
    }
    if (!cJSON_IsArray(data)) {
        cJSON_Delete(root);
        return false;
    }

    info->node_count = cJSON_GetArraySize(data);
    info->nodes_valid = true;
    cJSON_Delete(root);
    return true;
}

static const char *komari_json_skip_ws(const char *text)
{
    while (text != NULL && *text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    return text;
}

static const char *komari_json_value_after_key(const char *key_pos)
{
    if (key_pos == NULL) {
        return NULL;
    }

    const char *colon = strchr(key_pos, ':');
    if (colon == NULL) {
        return NULL;
    }
    return komari_json_skip_ws(colon + 1);
}

static bool komari_json_count_array_items(const char *array_start,
                                          int *count,
                                          const char **array_end)
{
    if (array_start == NULL || count == NULL || *array_start != '[') {
        return false;
    }

    int depth = 0;
    int item_count = 0;
    bool in_string = false;
    bool escaped = false;
    bool item_seen = false;

    for (const char *p = array_start; *p != '\0'; ++p) {
        char ch = *p;
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
            if (depth == 1) {
                item_seen = true;
            }
            continue;
        }

        if (ch == '[' || ch == '{') {
            if (depth == 1) {
                item_seen = true;
            }
            ++depth;
            continue;
        }

        if (ch == ']' || ch == '}') {
            if (ch == ']' && depth == 1) {
                if (item_seen) {
                    ++item_count;
                }
                *count = item_count;
                if (array_end != NULL) {
                    *array_end = p + 1;
                }
                return true;
            }
            if (depth <= 1) {
                return false;
            }
            --depth;
            continue;
        }

        if (depth == 1) {
            if (ch == ',') {
                if (!item_seen) {
                    return false;
                }
                ++item_count;
                item_seen = false;
            } else if (!isspace((unsigned char)ch)) {
                item_seen = true;
            }
        }
    }

    return false;
}

static bool komari_json_count_object_members(const char *object_start, int *count)
{
    if (object_start == NULL || count == NULL || *object_start != '{') {
        return false;
    }

    int depth = 0;
    int member_count = 0;
    bool in_string = false;
    bool escaped = false;
    bool member_seen = false;

    for (const char *p = object_start; *p != '\0'; ++p) {
        char ch = *p;
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
            if (depth == 1) {
                member_seen = true;
            }
            continue;
        }

        if (ch == '{' || ch == '[') {
            if (depth == 1) {
                member_seen = true;
            }
            ++depth;
            continue;
        }

        if (ch == '}' || ch == ']') {
            if (ch == '}' && depth == 1) {
                if (member_seen) {
                    ++member_count;
                }
                *count = member_count;
                return true;
            }
            if (depth <= 1) {
                return false;
            }
            --depth;
            continue;
        }

        if (depth == 1) {
            if (ch == ',') {
                if (!member_seen) {
                    return false;
                }
                ++member_count;
                member_seen = false;
            } else if (!isspace((unsigned char)ch)) {
                member_seen = true;
            }
        }
    }

    return false;
}

static bool komari_parse_clients_response(const char *response, komari_info_t *info)
{
    if (response == NULL || info == NULL) {
        return false;
    }

    const char *online_key = strstr(response, "\"online\"");
    const char *online_value = komari_json_value_after_key(online_key);
    if (online_value == NULL || *online_value != 0x5B) {
        return false;
    }

    int online_count = 0;
    const char *online_end = NULL;
    if (!komari_json_count_array_items(online_value, &online_count, &online_end)) {
        return false;
    }

    info->online_count = online_count;
    info->online_valid = true;

    if (info->node_count < 0 && online_end != NULL) {
        const char *nodes_key = strstr(online_end, "\"data\"");
        const char *nodes_value = komari_json_value_after_key(nodes_key);
        int node_count = 0;
        if (nodes_value != NULL && *nodes_value == 0x7B &&
            komari_json_count_object_members(nodes_value, &node_count)) {
            info->node_count = node_count;
            info->nodes_valid = true;
        }
    }

    return true;
}

static esp_err_t komari_fetch_public(const char *base_url, char *response,
                                     size_t response_len, komari_info_t *info)
{
    char url[KOMARI_URL_BUFFER_SIZE] = {0};
    ESP_RETURN_ON_FALSE(komari_build_api_url(base_url, "/api/public", url, sizeof(url)),
                        ESP_ERR_INVALID_SIZE, TAG, "build Komari public URL failed");

    esp_err_t ret = http_get_text_once(url, response, response_len, "komari-public");
    if (ret != ESP_OK) {
        return ret;
    }
    if (!komari_parse_public_response(response, info)) {
        ESP_LOGW(TAG, "Komari public parse failed: %.120s", response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t komari_fetch_nodes(const char *base_url, char *response,
                                    size_t response_len, komari_info_t *info)
{
    char url[KOMARI_URL_BUFFER_SIZE] = {0};
    ESP_RETURN_ON_FALSE(komari_build_api_url(base_url, "/api/nodes", url, sizeof(url)),
                        ESP_ERR_INVALID_SIZE, TAG, "build Komari nodes URL failed");

    esp_err_t ret = http_get_text_once(url, response, response_len, "komari-nodes");
    if (ret != ESP_OK) {
        return ret;
    }
    if (!komari_parse_nodes_response(response, info)) {
        ESP_LOGW(TAG, "Komari nodes parse failed: %.120s", response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

typedef struct {
    char *data;
    size_t len;
    size_t pos;
} komari_ws_prefetch_t;

static esp_err_t komari_transport_write_all(esp_transport_handle_t transport,
                                            const uint8_t *data, size_t len,
                                            int timeout_ms)
{
    size_t total = 0;
    while (total < len) {
        int written = esp_transport_write(transport, (const char *)data + total,
                                          (int)(len - total), timeout_ms);
        if (written <= 0) {
            ESP_LOGW(TAG, "Komari transport write failed: written=%d errno=%d", written, errno);
            return ESP_FAIL;
        }
        total += (size_t)written;
    }
    return ESP_OK;
}

static esp_err_t komari_ws_read_exact(esp_transport_handle_t transport,
                                      uint8_t *buffer, size_t len,
                                      komari_ws_prefetch_t *prefetch,
                                      int timeout_ms)
{
    size_t total = 0;
    while (total < len) {
        if (prefetch != NULL && prefetch->pos < prefetch->len) {
            size_t available = prefetch->len - prefetch->pos;
            size_t copy_len = len - total;
            if (copy_len > available) {
                copy_len = available;
            }
            memmove(buffer + total, prefetch->data + prefetch->pos, copy_len);
            prefetch->pos += copy_len;
            total += copy_len;
            continue;
        }

        int read_len = esp_transport_read(transport, (char *)buffer + total,
                                          (int)(len - total), timeout_ms);
        if (read_len > 0) {
            total += (size_t)read_len;
            continue;
        }
        ESP_LOGW(TAG, "Komari transport read failed: read=%d errno=%d", read_len, errno);
        return read_len == 0 ? ESP_ERR_TIMEOUT : ESP_FAIL;
    }
    return ESP_OK;
}

static void komari_ws_unmask(uint8_t *data, size_t len, const uint8_t mask[4])
{
    for (size_t i = 0; i < len; ++i) {
        data[i] ^= mask[i % 4];
    }
}

static bool komari_ws_build_host_header(const komari_url_parts_t *parts,
                                        char *host, size_t host_len)
{
    if (parts == NULL || host == NULL || host_len == 0) {
        return false;
    }

    int default_port = parts->https ? 443 : 80;
    int written = 0;
    if (parts->port == default_port) {
        written = snprintf(host, host_len, "%s", parts->host);
    } else {
        written = snprintf(host, host_len, "%s:%d", parts->host, parts->port);
    }
    return written > 0 && (size_t)written < host_len;
}

static bool komari_ws_build_origin_header(const komari_url_parts_t *parts,
                                          char *origin, size_t origin_len)
{
    if (parts == NULL || origin == NULL || origin_len == 0) {
        return false;
    }

    char host_header[KOMARI_HOST_LEN + 8] = {0};
    if (!komari_ws_build_host_header(parts, host_header, sizeof(host_header))) {
        return false;
    }

    int written = snprintf(origin, origin_len, "%s://%s",
                           parts->https ? "https" : "http", host_header);
    return written > 0 && (size_t)written < origin_len;
}

static bool komari_ws_status_is_101(const char *response)
{
    int status = 0;
    return response != NULL &&
           sscanf(response, "HTTP/%*[^ ] %d", &status) == 1 &&
           status == 101;
}

static esp_err_t komari_ws_make_key(char *key, size_t key_len)
{
    ESP_RETURN_ON_FALSE(key != NULL && key_len > 0, ESP_ERR_INVALID_ARG,
                        TAG, "invalid Komari WS key buffer");

    uint8_t random_key[16];
    esp_fill_random(random_key, sizeof(random_key));

    size_t written = 0;
    int ret = esp_crypto_base64_encode((unsigned char *)key, key_len, &written,
                                       random_key, sizeof(random_key));
    if (ret != 0 || written >= key_len) {
        ESP_LOGW(TAG, "Komari WS key encode failed: ret=%d written=%u",
                 ret, (unsigned)written);
        return ESP_FAIL;
    }
    key[written] = '\0';
    return ESP_OK;
}

static esp_err_t komari_ws_read_handshake_response(esp_transport_handle_t transport,
                                                   char *response,
                                                   size_t response_len,
                                                   size_t *prefetched_len)
{
    const char delimiter[] = "\r\n\r\n";
    size_t total = 0;

    ESP_RETURN_ON_FALSE(response != NULL && response_len > 1 && prefetched_len != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid Komari WS handshake buffer");

    *prefetched_len = 0;
    response[0] = '\0';
    while (total < response_len - 1) {
        int read_len = esp_transport_read(transport, response + total,
                                          (int)(response_len - 1 - total), 5000);
        if (read_len <= 0) {
            ESP_LOGW(TAG, "Komari WS handshake read failed: read=%d errno=%d", read_len, errno);
            return read_len == 0 ? ESP_ERR_TIMEOUT : ESP_FAIL;
        }

        total += (size_t)read_len;
        response[total] = '\0';

        char *header_end = strstr(response, delimiter);
        if (header_end == NULL) {
            continue;
        }

        size_t header_len = (size_t)(header_end - response) + strlen(delimiter);
        char saved = response[header_len];
        response[header_len] = '\0';
        bool status_ok = komari_ws_status_is_101(response);
        if (!status_ok) {
            ESP_LOGW(TAG, "Komari WS upgrade failed: %.120s", response);
            return ESP_ERR_INVALID_RESPONSE;
        }
        response[header_len] = saved;

        size_t extra_len = total - header_len;
        if (extra_len > 0) {
            memmove(response, response + header_len, extra_len);
        }
        response[extra_len] = '\0';
        *prefetched_len = extra_len;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Komari WS handshake header exceeded buffer: len=%u", (unsigned)total);
    return ESP_ERR_INVALID_SIZE;
}

static esp_err_t komari_ws_send_control_frame(esp_transport_handle_t transport,
                                              uint8_t opcode,
                                              const uint8_t *payload,
                                              size_t payload_len)
{
    ESP_RETURN_ON_FALSE(payload_len <= 125, ESP_ERR_INVALID_SIZE,
                        TAG, "Komari WS control payload too large");

    uint8_t frame[2 + 4 + 125];
    uint8_t mask[4];
    esp_fill_random(mask, sizeof(mask));

    frame[0] = 0x80 | (opcode & 0x0F);
    frame[1] = 0x80 | (uint8_t)payload_len;
    memcpy(frame + 2, mask, sizeof(mask));
    for (size_t i = 0; i < payload_len; ++i) {
        uint8_t byte = payload != NULL ? payload[i] : 0;
        frame[2 + sizeof(mask) + i] = byte ^ mask[i % sizeof(mask)];
    }

    return komari_transport_write_all(transport, frame, 2 + sizeof(mask) + payload_len, 5000);
}

static esp_err_t komari_ws_send_text_get(esp_transport_handle_t transport)
{
    const char payload[] = "get";
    const size_t payload_len = sizeof(payload) - 1;
    uint8_t frame[2 + 4 + sizeof(payload) - 1];
    uint8_t mask[4];

    esp_fill_random(mask, sizeof(mask));
    frame[0] = 0x81;
    frame[1] = 0x80 | (uint8_t)payload_len;
    memcpy(frame + 2, mask, sizeof(mask));
    for (size_t i = 0; i < payload_len; ++i) {
        frame[2 + sizeof(mask) + i] = ((uint8_t)payload[i]) ^ mask[i % sizeof(mask)];
    }

    return komari_transport_write_all(transport, frame, sizeof(frame), 5000);
}

static esp_err_t komari_ws_read_frame_header(esp_transport_handle_t transport,
                                             komari_ws_prefetch_t *prefetch,
                                             bool *fin,
                                             uint8_t *opcode,
                                             uint64_t *payload_len,
                                             bool *masked,
                                             uint8_t mask[4])
{
    uint8_t header[2] = {0};
    ESP_RETURN_ON_ERROR(komari_ws_read_exact(transport, header, sizeof(header),
                                             prefetch, 5000),
                        TAG, "read Komari WS frame header failed");

    *fin = (header[0] & 0x80) != 0;
    *opcode = header[0] & 0x0F;
    *masked = (header[1] & 0x80) != 0;
    *payload_len = header[1] & 0x7F;

    if (*payload_len == 126) {
        uint8_t ext[2] = {0};
        ESP_RETURN_ON_ERROR(komari_ws_read_exact(transport, ext, sizeof(ext),
                                                 prefetch, 5000),
                            TAG, "read Komari WS 16-bit length failed");
        *payload_len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (*payload_len == 127) {
        uint8_t ext[8] = {0};
        ESP_RETURN_ON_ERROR(komari_ws_read_exact(transport, ext, sizeof(ext),
                                                 prefetch, 5000),
                            TAG, "read Komari WS 64-bit length failed");
        *payload_len = 0;
        for (size_t i = 0; i < sizeof(ext); ++i) {
            *payload_len = (*payload_len << 8) | ext[i];
        }
    }

    if (*masked) {
        ESP_RETURN_ON_ERROR(komari_ws_read_exact(transport, mask, 4, prefetch, 5000),
                            TAG, "read Komari WS mask failed");
    } else {
        memset(mask, 0, 4);
    }
    return ESP_OK;
}

static esp_err_t komari_ws_read_text_frame(esp_transport_handle_t transport,
                                           char *response,
                                           size_t response_len,
                                           size_t prefetched_len)
{
    ESP_RETURN_ON_FALSE(response != NULL && response_len > 1, ESP_ERR_INVALID_ARG,
                        TAG, "invalid Komari WS response buffer");

    komari_ws_prefetch_t prefetch = {
        .data = response,
        .len = prefetched_len,
        .pos = 0,
    };

    size_t total = 0;
    for (int frame_index = 0; frame_index < 8; ++frame_index) {
        bool fin = false;
        bool masked = false;
        uint8_t opcode = 0;
        uint8_t mask[4] = {0};
        uint64_t payload_len = 0;

        ESP_RETURN_ON_ERROR(komari_ws_read_frame_header(transport, &prefetch,
                                                        &fin, &opcode,
                                                        &payload_len, &masked, mask),
                            TAG, "read Komari WS frame metadata failed");

        if ((opcode & 0x08) != 0) {
            if (payload_len > 125) {
                ESP_LOGW(TAG, "Komari WS invalid control length: %llu",
                         (unsigned long long)payload_len);
                return ESP_ERR_INVALID_RESPONSE;
            }

            uint8_t control_payload[125] = {0};
            if (payload_len > 0) {
                ESP_RETURN_ON_ERROR(komari_ws_read_exact(transport, control_payload,
                                                         (size_t)payload_len,
                                                         &prefetch, 5000),
                                    TAG, "read Komari WS control payload failed");
                if (masked) {
                    komari_ws_unmask(control_payload, (size_t)payload_len, mask);
                }
            }

            if (opcode == 0x8) {
                ESP_LOGW(TAG, "Komari WS closed by server");
                return ESP_FAIL;
            }
            if (opcode == 0x9) {
                ESP_RETURN_ON_ERROR(komari_ws_send_control_frame(transport, 0xA,
                                                                 control_payload,
                                                                 (size_t)payload_len),
                                    TAG, "send Komari WS pong failed");
            }
            continue;
        }

        if (opcode != 0x1 && opcode != 0x0) {
            ESP_LOGW(TAG, "Komari WS unsupported opcode: %u", opcode);
            return ESP_ERR_INVALID_RESPONSE;
        }

        if (payload_len > (uint64_t)(response_len - 1 - total)) {
            ESP_LOGW(TAG, "Komari WS payload too large: %llu",
                     (unsigned long long)payload_len);
            return ESP_ERR_INVALID_SIZE;
        }

        if (payload_len > 0) {
            ESP_RETURN_ON_ERROR(komari_ws_read_exact(transport,
                                                     (uint8_t *)response + total,
                                                     (size_t)payload_len,
                                                     &prefetch, 5000),
                                TAG, "read Komari WS payload failed");
            if (masked) {
                komari_ws_unmask((uint8_t *)response + total,
                                 (size_t)payload_len, mask);
            }
            total += (size_t)payload_len;
            response[total] = '\0';
        }

        if (fin) {
            return total > 0 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
        }
    }

    ESP_LOGW(TAG, "Komari WS text frame did not finish");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t komari_fetch_online_ws(const char *base_url, char *response,
                                        size_t response_len, komari_info_t *info)
{
    ESP_RETURN_ON_FALSE(response != NULL && response_len > 1, ESP_ERR_INVALID_ARG,
                        TAG, "invalid Komari WS buffer");

    komari_url_parts_t parts = {0};
    ESP_RETURN_ON_FALSE(komari_parse_base_url(base_url, &parts), ESP_ERR_INVALID_ARG,
                        TAG, "invalid Komari base URL");

    char ws_path[KOMARI_PATH_LEN + 32] = {0};
    ESP_RETURN_ON_FALSE(komari_build_ws_path(&parts, ws_path, sizeof(ws_path)),
                        ESP_ERR_INVALID_SIZE, TAG, "build Komari WS path failed");

    esp_transport_handle_t transport = parts.https ? esp_transport_ssl_init() : esp_transport_tcp_init();
    ESP_RETURN_ON_FALSE(transport != NULL, ESP_ERR_NO_MEM, TAG, "create Komari transport failed");
    if (parts.https) {
        esp_transport_ssl_crt_bundle_attach(transport, esp_crt_bundle_attach);
    }

    esp_err_t ret = ESP_OK;
    if (esp_transport_connect(transport, parts.host, parts.port, 8000) != 0) {
        ESP_LOGW(TAG, "Komari WS transport connect failed: host=%s port=%d path=%s errno=%d",
                 parts.host, parts.port, ws_path, errno);
        ret = ESP_FAIL;
        goto cleanup;
    }

    char host_header[KOMARI_HOST_LEN + 8] = {0};
    ESP_GOTO_ON_FALSE(komari_ws_build_host_header(&parts, host_header, sizeof(host_header)),
                      ESP_ERR_INVALID_SIZE, cleanup, TAG, "build Komari WS Host header failed");

    char origin_header[KOMARI_HOST_LEN + 16] = {0};
    ESP_GOTO_ON_FALSE(komari_ws_build_origin_header(&parts, origin_header, sizeof(origin_header)),
                      ESP_ERR_INVALID_SIZE, cleanup, TAG, "build Komari WS Origin header failed");

    char ws_key[32] = {0};
    ESP_GOTO_ON_ERROR(komari_ws_make_key(ws_key, sizeof(ws_key)),
                      cleanup, TAG, "make Komari WS key failed");

    char request[512] = {0};
    int request_len = snprintf(request, sizeof(request),
                               "GET %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Key: %s\r\n"
                               "Sec-WebSocket-Version: 13\r\n"
                               "Origin: %s\r\n"
                               "User-Agent: esp32-cyd-clock\r\n"
                               "\r\n",
                               ws_path, host_header, ws_key, origin_header);
    ESP_GOTO_ON_FALSE(request_len > 0 && (size_t)request_len < sizeof(request),
                      ESP_ERR_INVALID_SIZE, cleanup, TAG, "build Komari WS request failed");

    ESP_GOTO_ON_ERROR(komari_transport_write_all(transport,
                                                 (const uint8_t *)request,
                                                 (size_t)request_len, 5000),
                      cleanup, TAG, "send Komari WS handshake failed");

    size_t prefetched_len = 0;
    ESP_GOTO_ON_ERROR(komari_ws_read_handshake_response(transport, response,
                                                        response_len,
                                                        &prefetched_len),
                      cleanup, TAG, "read Komari WS handshake failed");

    ESP_GOTO_ON_ERROR(komari_ws_send_text_get(transport),
                      cleanup, TAG, "send Komari WS get failed");

    ret = komari_ws_read_text_frame(transport, response, response_len, prefetched_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Komari WS read text failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    if (!komari_parse_clients_response(response, info)) {
        ESP_LOGW(TAG, "Komari WS parse failed: %.120s", response);
        ret = ESP_ERR_INVALID_RESPONSE;
        goto cleanup;
    }

cleanup:
    (void)esp_transport_close(transport);
    esp_transport_destroy(transport);
    return ret;
}

static esp_err_t komari_fetch_once(void)
{
    char base_url[KOMARI_URL_LEN] = {0};
    if (!komari_get_base_url(base_url, sizeof(base_url))) {
        return ESP_ERR_NOT_FOUND;
    }

    char *response = heap_caps_calloc(1, KOMARI_WS_BUFFER_SIZE, MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(response != NULL, ESP_ERR_NO_MEM, TAG, "alloc Komari response failed");

    komari_info_t info = {0};
    info.configured = true;
    info.online_count = -1;
    info.node_count = -1;
    strlcpy(info.base_url, base_url, sizeof(info.base_url));
    komari_set_default_text(&info);

    esp_err_t ret = komari_fetch_public(base_url, response, KOMARI_HTTP_BUFFER_SIZE, &info);
    if (ret != ESP_OK) {
        free(response);
        return ret;
    }

    memset(response, 0, KOMARI_WS_BUFFER_SIZE);
    esp_err_t nodes_ret = komari_fetch_nodes(base_url, response, KOMARI_HTTP_BUFFER_SIZE, &info);
    if (nodes_ret != ESP_OK) {
        ESP_LOGW(TAG, "Komari nodes update failed: %s", esp_err_to_name(nodes_ret));
    }

    memset(response, 0, KOMARI_WS_BUFFER_SIZE);
    esp_err_t online_ret = komari_fetch_online_ws(base_url, response, KOMARI_WS_BUFFER_SIZE, &info);
    if (online_ret != ESP_OK) {
        ESP_LOGW(TAG, "Komari online update failed: %s", esp_err_to_name(online_ret));
    }
    free(response);

    if (info.online_valid && info.node_count >= 0 && info.online_count > info.node_count) {
        info.node_count = info.online_count;
    }
    komari_set_snapshot(&info);
    ESP_LOGI(TAG, "komari updated: online=%d total=%d site=%s public=%d nodes=%d online_valid=%d",
             info.online_count, info.node_count, info.sitename,
             info.public_valid, info.nodes_valid, info.online_valid);
    return ESP_OK;
}

static void komari_task(void *arg)
{
    (void)arg;

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

        if (!komari_configured()) {
            vTaskDelay(pdMS_TO_TICKS(KOMARI_REFRESH_MS));
            continue;
        }

        esp_err_t ret = ESP_ERR_TIMEOUT;
        if (network_fetch_lock_take("komari", pdMS_TO_TICKS(120000))) {
            ret = komari_fetch_once();
            network_fetch_lock_give();
        }
        if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "komari update failed: %s", esp_err_to_name(ret));
        }
        log_runtime_health("komari");
        vTaskDelay(pdMS_TO_TICKS(KOMARI_REFRESH_MS));
    }
}

static esp_err_t weather_fetch_url(const char *url, weather_info_t *out, const char *label)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid weather output");

    char response[WEATHER_TEXT_HTTP_BUFFER_SIZE] = {0};
    esp_err_t ret = http_get_text(url, response, sizeof(response), label);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!weather_parse_response(response, out)) {
        ESP_LOGW(TAG, "%s parse failed", label != NULL ? label : "weather");
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static void weather_apply_geo_city(weather_info_t *parsed, const weather_geo_info_t *geo,
                                   const char *city)
{
    if (parsed == NULL || geo == NULL) {
        return;
    }
    if (geo->city_display[0] != '\0') {
        strlcpy(parsed->city, geo->city_display, sizeof(parsed->city));
    } else if (city != NULL && city[0] != '\0') {
        sanitize_weather_city(city, parsed->city, sizeof(parsed->city));
    }
}

static esp_err_t weather_fetch_open_meteo_location(const weather_geo_info_t *geo,
                                                   double *latitude, double *longitude,
                                                   char *city, size_t city_len)
{
    ESP_RETURN_ON_FALSE(geo != NULL && latitude != NULL && longitude != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid Open-Meteo location request");

    const char *city_query = geo->city_native[0] != '\0' ? geo->city_native : geo->city_query;
    if (city_query == NULL || city_query[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char url[WEATHER_URL_BUFFER_SIZE] = {0};
    if (!weather_build_open_meteo_geocode_url(city_query, url, sizeof(url))) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *response = heap_caps_calloc(1, WEATHER_HTTP_BUFFER_SIZE, MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(response != NULL, ESP_ERR_NO_MEM, TAG, "alloc Open-Meteo geocode response failed");

    esp_err_t ret = http_get_text(url, response, WEATHER_HTTP_BUFFER_SIZE, "meteo-geo");
    if (ret != ESP_OK) {
        free(response);
        return ret;
    }

    if (!weather_parse_open_meteo_geocode_response(response, geo, latitude, longitude,
                                                   city, city_len)) {
        ESP_LOGW(TAG, "Open-Meteo geocode parse failed: city=%s response=%.120s",
                 city_query, response);
        free(response);
        return ESP_ERR_INVALID_RESPONSE;
    }

    free(response);
    return ESP_OK;
}

static esp_err_t weather_fetch_open_meteo_forecast(double latitude, double longitude,
                                                   weather_info_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid Open-Meteo output");

    char url[WEATHER_URL_BUFFER_SIZE] = {0};
    if (!weather_build_open_meteo_forecast_url(latitude, longitude, url, sizeof(url))) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *response = heap_caps_calloc(1, WEATHER_HTTP_BUFFER_SIZE, MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(response != NULL, ESP_ERR_NO_MEM, TAG, "alloc Open-Meteo forecast response failed");

    esp_err_t ret = http_get_text(url, response, WEATHER_HTTP_BUFFER_SIZE, "meteo-weather");
    if (ret != ESP_OK) {
        free(response);
        return ret;
    }

    weather_info_t parsed = {0};
    parsed.weather_code = -1;
    if (!weather_parse_open_meteo_forecast_response(response, &parsed)) {
        ESP_LOGW(TAG, "Open-Meteo weather parse failed: %.120s", response);
        free(response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    free(response);

    *out = parsed;
    return ESP_OK;
}

static esp_err_t weather_fetch_open_meteo_hourly(double latitude, double longitude,
                                                 weather_info_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid Open-Meteo hourly output");

    char url[WEATHER_URL_BUFFER_SIZE] = {0};
    if (!weather_build_open_meteo_hourly_url(latitude, longitude, url, sizeof(url))) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *response = heap_caps_calloc(1, WEATHER_HOURLY_HTTP_BUFFER_SIZE, MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(response != NULL, ESP_ERR_NO_MEM, TAG, "alloc Open-Meteo hourly response failed");

    esp_err_t ret = http_get_text(url, response, WEATHER_HOURLY_HTTP_BUFFER_SIZE, "meteo-hourly");
    if (ret != ESP_OK) {
        free(response);
        return ret;
    }

    if (!weather_parse_open_meteo_hourly_response(response, out)) {
        ESP_LOGW(TAG, "Open-Meteo hourly parse failed: %.120s", response);
        free(response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    free(response);

    ESP_LOGI(TAG, "Open-Meteo hourly selected: lat=%.4f lon=%.4f hours=%d",
             latitude, longitude, out->hourly_count);
    return ESP_OK;
}

static esp_err_t weather_fetch_7timer_forecast(double latitude, double longitude,
                                               weather_info_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid 7Timer output");

    char url[WEATHER_URL_BUFFER_SIZE] = {0};
    if (!weather_build_7timer_forecast_url(latitude, longitude, url, sizeof(url))) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *response = heap_caps_calloc(1, WEATHER_FORECAST_HTTP_BUFFER_SIZE, MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(response != NULL, ESP_ERR_NO_MEM, TAG, "alloc 7Timer response failed");

    esp_err_t ret = http_get_text(url, response, WEATHER_FORECAST_HTTP_BUFFER_SIZE, "7timer-weather");
    if (ret != ESP_OK) {
        free(response);
        return ret;
    }

    weather_info_t parsed = {0};
    parsed.weather_code = -1;
    if (!weather_parse_7timer_forecast_response(response, &parsed)) {
        ESP_LOGW(TAG, "7Timer forecast parse failed: %.120s", response);
        free(response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    free(response);

    ESP_LOGI(TAG, "7Timer forecast selected: lat=%.4f lon=%.4f days=%d",
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

    double latitude = 0.0;
    double longitude = 0.0;
    char open_meteo_city[WEATHER_CITY_LEN] = {0};
    weather_info_t backup_forecast = {0};
    esp_err_t forecast_ret = ESP_ERR_NOT_SUPPORTED;
    ret = weather_fetch_open_meteo_location(&geo, &latitude, &longitude,
                                            open_meteo_city, sizeof(open_meteo_city));
    if (ret == ESP_OK) {
        weather_info_t parsed = {0};
        ret = weather_fetch_open_meteo_forecast(latitude, longitude, &parsed);
        if (ret == ESP_OK) {
            weather_apply_geo_city(&parsed, &geo, open_meteo_city);
            esp_err_t hourly_ret = weather_fetch_open_meteo_hourly(latitude, longitude, &parsed);
            if (hourly_ret != ESP_OK) {
                ESP_LOGI(TAG, "Open-Meteo hourly failed: %s, keep daily forecast only",
                         esp_err_to_name(hourly_ret));
            }
            ESP_LOGI(TAG, "Open-Meteo selected: city=%s lat=%.4f lon=%.4f days=%d hours=%d",
                     parsed.city[0] != '\0' ? parsed.city : geo.city_query,
                     latitude, longitude, parsed.forecast_count, parsed.hourly_count);
            *out = parsed;
            return ESP_OK;
        }

        ESP_LOGI(TAG, "Open-Meteo forecast failed: %s, fallback to 7Timer forecast",
                 esp_err_to_name(ret));
        forecast_ret = weather_fetch_7timer_forecast(latitude, longitude, &backup_forecast);
        if (forecast_ret != ESP_OK) {
            ESP_LOGI(TAG, "7Timer forecast failed: %s, fallback to wttr current weather",
                     esp_err_to_name(forecast_ret));
        }
    } else {
        ESP_LOGI(TAG, "Open-Meteo geocode failed: %s, fallback to wttr city weather",
                 esp_err_to_name(ret));
    }

    weather_info_t parsed = {0};
    ret = weather_fetch_url(weather_url, &parsed, "weather-cn");
    if (ret != ESP_OK) {
        return ret;
    }
    if (forecast_ret == ESP_OK) {
        weather_copy_forecast(&parsed, &backup_forecast);
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
        ESP_LOGI(TAG, "weather updated[%s]: city=%s temp=%d humidity=%d condition=%s days=%d hours=%d",
                 source, parsed.city[0] != '\0' ? parsed.city : "-",
                 parsed.temp_c, parsed.humidity, parsed.condition,
                 parsed.forecast_count, parsed.hourly_count);
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

        esp_err_t ret = ESP_ERR_TIMEOUT;
        if (network_fetch_lock_take("weather", pdMS_TO_TICKS(120000))) {
            ret = weather_fetch_once();
            network_fetch_lock_give();
        }
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "weather update failed: %s", esp_err_to_name(ret));
        }
        log_runtime_health("weather");
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

static uint16_t humidity_ring_color(int humidity)
{
    if (humidity < 0) {
        humidity = 0;
    } else if (humidity > 100) {
        humidity = 100;
    }

    if (humidity <= 45) {
        return mix_color(rgb565(45, 147, 255), rgb565(30, 226, 214), humidity, 45);
    }
    if (humidity <= 75) {
        return mix_color(rgb565(30, 226, 214), rgb565(76, 235, 142), humidity - 45, 30);
    }
    return mix_color(rgb565(76, 235, 142), rgb565(255, 211, 85), humidity - 75, 25);
}

static esp_err_t draw_card_icon(int kind, int cx, int cy, uint16_t accent, int humidity)
{
    const uint16_t icon_bg = mix_color(rgb565(8, 12, 33), accent, 2, 5);
    const uint16_t white = rgb565(216, 244, 255);
    const int bg_radius = 8;

    if (kind == 0) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, bg_radius, icon_bg), TAG, "draw icon bg failed");
        ESP_RETURN_ON_ERROR(lcd_draw_rect_outline(cx - 5, cy - 5, 10, 10, accent), TAG,
                            "draw clock outline failed");
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 1, white), TAG, "draw clock center failed");
        ESP_RETURN_ON_ERROR(lcd_draw_line(cx, cy, cx, cy - 4, 1, white), TAG, "draw clock hand failed");
        return lcd_draw_line(cx, cy, cx + 4, cy + 3, 1, white);
    }
    if (kind == 1) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, bg_radius, icon_bg), TAG, "draw icon bg failed");
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(cx - 6, cy - 6, 12, 11, 2, accent), TAG,
                            "draw calendar failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - 4, cy - 2, 8, 1, icon_bg), TAG, "draw calendar line failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - 4, cy + 2, 6, 1, icon_bg), TAG, "draw calendar line failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - 4, cy - 9, 2, 5, white), TAG, "draw calendar pin failed");
        return lcd_fill_rect(cx + 3, cy - 9, 2, 5, white);
    }
    if (kind == 4) {
        // 更多：2x2 小方块网格，右下角用白色点出"应用"感。
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, bg_radius, icon_bg), TAG, "draw more icon bg failed");
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(cx - 6, cy - 6, 5, 5, 1, accent), TAG, "draw more icon failed");
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(cx + 1, cy - 6, 5, 5, 1, accent), TAG, "draw more icon failed");
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(cx - 6, cy + 1, 5, 5, 1, accent), TAG, "draw more icon failed");
        return lcd_fill_round_rect(cx + 1, cy + 1, 5, 5, 1, white);
    }
    if (kind == 3) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, bg_radius, icon_bg), TAG, "draw komari icon bg failed");
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 5, cy - 3, 2, accent), TAG, "draw komari node failed");
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx + 5, cy - 3, 2, accent), TAG, "draw komari node failed");
        ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy + 5, 2, white), TAG, "draw komari node failed");
        ESP_RETURN_ON_ERROR(lcd_draw_line(cx - 4, cy - 2, cx - 1, cy + 3, 1, accent),
                            TAG, "draw komari link failed");
        return lcd_draw_line(cx + 4, cy - 2, cx + 1, cy + 3, 1, accent);
    }

    const uint16_t ring_base = mix_color(rgb565(18, 26, 55), accent, 1, 5);
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, bg_radius, icon_bg), TAG, "draw icon bg failed");
    ESP_RETURN_ON_ERROR(lcd_draw_arc(cx, cy, 10, -90, 270, 2, ring_base),
                        TAG, "draw humidity ring base failed");
    if (humidity >= 0) {
        if (humidity > 100) {
            humidity = 100;
        }
        int sweep = (humidity * 360) / 100;
        if (sweep > 0) {
            ESP_RETURN_ON_ERROR(lcd_draw_arc(cx, cy, 10, -90, -90 + sweep, 2,
                                             humidity_ring_color(humidity)),
                                TAG, "draw humidity ring failed");
        }
    }
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(cx, cy - 7, cx - 5, cy + 1, cx + 5, cy + 1, accent),
                        TAG, "draw drop top failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy + 2, 5, accent), TAG, "draw drop body failed");
    return lcd_draw_line(cx + 2, cy + 5, cx + 5, cy + 2, 1, rgb565(10, 78, 92));
}

static esp_err_t draw_metric_card(int x, int y, int w, int h,
                                  const char *label, const char *value,
                                  int value_scale, uint16_t accent,
                                  int icon_kind, int humidity)
{
    const uint16_t border = mix_color(rgb565(12, 21, 50), accent, 1, 2);
    const uint16_t card = mix_color(rgb565(5, 10, 28), accent, 1, 6);
    const uint16_t label_color = rgb565(210, 225, 244);
    int scale = value_scale;
    const bool compact = w < 72;
    const int icon_x = x + (compact ? 12 : 17);
    const int label_x = x + (compact ? 25 : 32);
    const int label_w = w - (compact ? 29 : 36);

    if (text5_width(value, scale) > w - 8) {
        scale = 1;
    }

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, w, h, 8, border), TAG, "draw metric card border failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 1, y + 1, w - 2, h - 2, 7, card),
                        TAG, "draw metric card failed");
    ESP_RETURN_ON_ERROR(draw_card_icon(icon_kind, icon_x, y + 10, accent, humidity),
                        TAG, "draw card icon failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(label_x, y + 4, label, label_w, 1, label_color),
                        TAG, "draw metric label failed");

    int value_x = x + (w - text5_width(value, scale)) / 2;
    int value_y = y + h - (scale > 1 ? 16 : 12);
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

static void nav_card_rect(int index, int count, int *x, int *w)
{
    const int total_w = CLOCK_PANEL_W - 28;
    const int gap = count >= 4 ? 6 : NAV_CARD_GAP;
    int card_w = (total_w - gap * (count - 1)) / count;
    if (card_w <= 0) {
        card_w = NAV_CARD_W;
    }
    if (x != NULL) {
        *x = CLOCK_PANEL_X + 14 + index * (card_w + gap);
    }
    if (w != NULL) {
        *w = card_w;
    }
}

static int nav_card_count(bool show_komari)
{
    (void)show_komari;
    return 3;
}

static app_view_t nav_card_target_view(int index, bool show_komari)
{
    switch (index) {
    case 0:
        return APP_VIEW_MORE;
    case 1:
        return show_komari ? APP_VIEW_WEATHER : APP_VIEW_CALENDAR;
    case 2:
        return show_komari ? APP_VIEW_KOMARI : APP_VIEW_WEATHER;
    default:
        return APP_VIEW_DASHBOARD;
    }
}

static esp_err_t draw_nav_cards(const struct tm *tm, app_view_t current_view,
                                const weather_info_t *weather)
{
    char more_buf[16];
    char short_date_buf[32];
    char hum_buf[16];
    char komari_buf[16];
    int humidity = -1;
    int card_x = 0;
    int card_w = 0;
    bool show_komari = komari_configured();
    int card_count = nav_card_count(show_komari);
    komari_info_t komari = {0};

    // 更多卡片：有计时在运行时直接显示剩余时间，否则显示省略号。
    if (s_pomodoro_timer.running) {
        format_timer_mmss(focus_timer_remaining_seconds(&s_pomodoro_timer), more_buf, sizeof(more_buf));
    } else if (s_countdown_timer.running) {
        format_timer_mmss(focus_timer_remaining_seconds(&s_countdown_timer), more_buf, sizeof(more_buf));
    } else {
        strlcpy(more_buf, "...", sizeof(more_buf));
    }
    snprintf(short_date_buf, sizeof(short_date_buf), "%02d/%02d", tm->tm_mon + 1, tm->tm_mday);
    if (weather != NULL && weather->valid) {
        snprintf(hum_buf, sizeof(hum_buf), "%d%%", weather->humidity);
        humidity = weather->humidity;
    } else {
        strlcpy(hum_buf, "--%", sizeof(hum_buf));
    }

    if (show_komari) {
        komari_get_snapshot(&komari);
        if (!komari.valid) {
            strlcpy(komari_buf, "WAIT", sizeof(komari_buf));
        } else if (komari.online_valid && komari.node_count >= 0) {
            snprintf(komari_buf, sizeof(komari_buf), "%d/%d", komari.online_count, komari.node_count);
        } else if (komari.online_valid) {
            snprintf(komari_buf, sizeof(komari_buf), "%d", komari.online_count);
        } else if (komari.nodes_valid) {
            snprintf(komari_buf, sizeof(komari_buf), "--/%d", komari.node_count);
        } else {
            strlcpy(komari_buf, "ERR", sizeof(komari_buf));
        }
    }

    nav_card_rect(0, card_count, &card_x, &card_w);
    ESP_RETURN_ON_ERROR(draw_metric_card(card_x, NAV_CARD_Y, card_w, NAV_CARD_H,
                                         "更多", more_buf, NAV_CARD_VALUE_SCALE,
                                         nav_accent(current_view, APP_VIEW_MORE,
                                                    rgb565(45, 147, 255), rgb565(105, 207, 255)),
                                         4, -1),
                        TAG, "draw more nav failed");
    if (!show_komari) {
        nav_card_rect(1, card_count, &card_x, &card_w);
        ESP_RETURN_ON_ERROR(draw_metric_card(card_x, NAV_CARD_Y, card_w, NAV_CARD_H,
                                             "日期", short_date_buf, NAV_CARD_VALUE_SCALE,
                                             nav_accent(current_view, APP_VIEW_CALENDAR,
                                                        rgb565(186, 85, 255), rgb565(227, 124, 255)),
                                             1, -1),
                            TAG, "draw date nav failed");
    }

    nav_card_rect(show_komari ? 1 : 2, card_count, &card_x, &card_w);
    ESP_RETURN_ON_ERROR(draw_metric_card(card_x, NAV_CARD_Y, card_w, NAV_CARD_H,
                                         "湿度", hum_buf, NAV_CARD_VALUE_SCALE,
                                         nav_accent(current_view, APP_VIEW_WEATHER,
                                                    rgb565(30, 226, 214), rgb565(95, 255, 221)),
                                         2, humidity),
                        TAG, "draw humidity nav failed");
    if (!show_komari) {
        return ESP_OK;
    }

    nav_card_rect(2, card_count, &card_x, &card_w);
    return draw_metric_card(card_x, NAV_CARD_Y, card_w, NAV_CARD_H,
                            "KOMARI", komari_buf, NAV_CARD_VALUE_SCALE,
                            nav_accent(current_view, APP_VIEW_KOMARI,
                                       rgb565(255, 185, 85), rgb565(255, 220, 125)),
                            3, -1);
}

static esp_err_t draw_location_pin(int cx, int cy, uint16_t color, uint16_t hole)
{
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 3, color), TAG, "draw pin head failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(cx - 3, cy + 1, cx + 3, cy + 1, cx, cy + 7, color),
                        TAG, "draw pin tip failed");
    return lcd_fill_rect(cx, cy, 1, 1, hole);
}

static esp_err_t draw_health_chip(int x, int y, int w, int h, bool healthy)
{
    const uint16_t border = rgb565(52, 66, 118);
    const uint16_t fill = rgb565(14, 20, 48);
    const uint16_t dot = healthy ? rgb565(76, 235, 142) : rgb565(255, 196, 76);
    const uint16_t text = rgb565(196, 210, 238);

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, w, h, h / 2, border), TAG, "draw chip border failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 1, y + 1, w - 2, h - 2, h / 2 - 1, fill),
                        TAG, "draw chip fill failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(x + 10, y + h / 2, 4, mix_color(fill, dot, 2, 5)),
                        TAG, "draw chip glow failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(x + 10, y + h / 2, 2, dot), TAG, "draw chip dot failed");
    return draw_text_mixed_limited(x + 19, y + (h - 16) / 2, "状态", w - 22, 1, text);
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
        snprintf(temp_buf, sizeof(temp_buf), "%d" DEG_SIGN, weather.temp_c);
        condition_text = weather_condition_zh(weather.condition);
        if (weather.city[0] != '\0') {
            center_text = weather.city;
        }
    } else {
        strlcpy(temp_buf, "--" DEG_SIGN, sizeof(temp_buf));
        condition_text = s_prov_active ? "配网" : "等待";
    }

    // 先在内存中合成完整面板，再分块刷屏，避免清屏和逐元素重画被人眼看到。
    ESP_RETURN_ON_ERROR(lcd_canvas_begin(CLOCK_PANEL_X, CLOCK_PANEL_Y, CLOCK_PANEL_W, CLOCK_PANEL_H),
                        TAG, "begin canvas failed");
    ESP_RETURN_ON_ERROR(lcd_canvas_fill_gradient(rgb565(4, 7, 21), rgb565(5, 9, 28)),
                        TAG, "fill canvas failed");
    ESP_RETURN_ON_ERROR(draw_panel_frame(), TAG, "draw panel failed");
    ESP_RETURN_ON_ERROR(draw_scenic_background(), TAG, "draw scenic background failed");

    // 顶部状态行：左侧 Wi-Fi，中间城市，右侧天气。
    ESP_RETURN_ON_ERROR(draw_wifi_icon(CLOCK_PANEL_X + 12, CLOCK_PANEL_Y + 6,
                                       s_wifi_connected || s_prov_active),
                        TAG, "draw wifi icon failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(CLOCK_PANEL_X + 42, CLOCK_PANEL_Y + 8, status_text,
                                                104, 1, s_prov_active ? yellow : text),
                        TAG, "draw status text failed");
    int center_w = text_mixed_width(center_text, 1);
    if (center_w > 110) {
        center_w = 110;
    }
    const int center_x = 160 - (center_w + 10) / 2;
    ESP_RETURN_ON_ERROR(draw_location_pin(center_x + 3, CLOCK_PANEL_Y + 29, rgb565(255, 120, 170),
                                          rgb565(10, 14, 40)),
                        TAG, "draw location pin failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(center_x + 10, CLOCK_PANEL_Y + 27, center_text,
                                                110, 1, muted),
                        TAG, "draw center text failed");

    int temp_x = CLOCK_PANEL_X + CLOCK_PANEL_W - text5_width(temp_buf, 2) - 10;
    int weather_icon_x = temp_x - 18;
    ESP_RETURN_ON_ERROR(draw_dashboard_weather_icon(weather.valid ? weather.condition : NULL,
                                                    weather_icon_x, CLOCK_PANEL_Y + 16,
                                                    yellow, muted),
                        TAG, "draw dashboard weather icon failed");
    ESP_RETURN_ON_ERROR(draw_text5(temp_x, CLOCK_PANEL_Y + 10, temp_buf, 2, text),
                        TAG, "draw temperature failed");
    int condition_w = text_mixed_width(condition_text, 1);
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(CLOCK_PANEL_X + CLOCK_PANEL_W - 10 - condition_w,
                                                CLOCK_PANEL_Y + 29, condition_text,
                                                60, 1, muted),
                        TAG, "draw weather text failed");

    ESP_RETURN_ON_ERROR(draw_time_digits(tm), TAG, "draw time failed");
    ESP_RETURN_ON_ERROR(draw_progress_dots(tm->tm_sec), TAG, "draw seconds progress failed");

    // 日期行：左侧日期和星期，右侧设备状态入口。
    const int date_y = CLOCK_PANEL_Y + 127;
    const int date_x = CLOCK_PANEL_X + 18;
    ESP_RETURN_ON_ERROR(draw_text5(date_x, date_y, date_buf, 2, text),
                        TAG, "draw date failed");
    ESP_RETURN_ON_ERROR(lcd_fill_rect(date_x + text5_width(date_buf, 2) + 2, date_y + 2, 1, 10,
                                      rgb565(60, 72, 120)),
                        TAG, "draw date divider failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed(date_x + text5_width(date_buf, 2) + 10, date_y - 1,
                                        weekday_name(tm->tm_wday), 1, rgb565(176, 150, 255)),
                        TAG, "draw weekday failed");
    ESP_RETURN_ON_ERROR(draw_health_chip(CLOCK_PANEL_X + CLOCK_PANEL_W - 70, date_y - 3, 60, 20,
                                         s_wifi_connected && weather.valid),
                        TAG, "draw health chip failed");

    ESP_RETURN_ON_ERROR(draw_nav_cards(tm, APP_VIEW_DASHBOARD, &weather), TAG, "draw nav cards failed");
    return lcd_canvas_flush();
}

#define VIEW_HEADER_H 36

static esp_err_t draw_view_background(uint16_t top, uint16_t bottom)
{
    ESP_RETURN_ON_ERROR(lcd_canvas_begin(CLOCK_PANEL_X, CLOCK_PANEL_Y, CLOCK_PANEL_W, CLOCK_PANEL_H),
                        TAG, "begin view canvas failed");
    ESP_RETURN_ON_ERROR(lcd_canvas_fill_gradient(top, bottom), TAG, "fill view canvas failed");
    ESP_RETURN_ON_ERROR(draw_panel_frame(), TAG, "draw view panel failed");
    ESP_RETURN_ON_ERROR(lcd_fill_panel_blend_rows(CLOCK_PANEL_Y + VIEW_HEADER_H + 1,
                                                  CLOCK_PANEL_H - VIEW_HEADER_H - 2, top, bottom),
                        TAG, "fill view body failed");
    return lcd_fill_panel_blend_rows(CLOCK_PANEL_Y + 1, VIEW_HEADER_H,
                                     mix_color(top, rgb565(30, 46, 92), 1, 2), top);
}

static esp_err_t draw_view_header(const char *title, const struct tm *tm, uint16_t accent)
{
    char time_buf[8];
    const uint16_t text = rgb565(240, 245, 255);
    const uint16_t muted = rgb565(142, 158, 191);
    const uint16_t yellow = rgb565(255, 211, 85);
    const uint16_t line = rgb565(32, 46, 84);
    const char *status_text = s_prov_active ? "配网" :
                              (!s_wifi_configured ? "未配网" :
                               (s_wifi_connected && s_connected_ssid[0] != '\0' ? s_connected_ssid : "连接中"));
    const int line_y = CLOCK_PANEL_Y + VIEW_HEADER_H;
    int title_w = text_mixed_width(title, 1);

    if (title_w > 74) {
        title_w = 74;
    }

    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
    ESP_RETURN_ON_ERROR(draw_wifi_icon(CLOCK_PANEL_X + 10, CLOCK_PANEL_Y + 4,
                                       s_wifi_connected || s_prov_active),
                        TAG, "draw view wifi failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(CLOCK_PANEL_X + 40, CLOCK_PANEL_Y + 9,
                                                status_text, 72, 1,
                                                s_prov_active ? yellow : muted),
                        TAG, "draw view wifi text failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_centered_limited(160, CLOCK_PANEL_Y + 9,
                                                         title, 74, 1, text),
                        TAG, "draw view title failed");

    // 分隔线中间用页面主题色高亮，标题下方带一段短强调条。
    ESP_RETURN_ON_ERROR(lcd_draw_hline(CLOCK_PANEL_X + 8, line_y, CLOCK_PANEL_W - 16, line),
                        TAG, "draw view header line failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(160 - title_w / 2 - 4, line_y - 1, title_w + 8, 3, 1, accent),
                        TAG, "draw view title accent failed");

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(CLOCK_PANEL_X + CLOCK_PANEL_W - 52, CLOCK_PANEL_Y + 9,
                                            42, 17, 8, mix_color(rgb565(12, 18, 40), accent, 1, 6)),
                        TAG, "draw view time chip failed");
    return draw_text5(CLOCK_PANEL_X + CLOCK_PANEL_W - 31 - text5_width(time_buf, 1) / 2,
                      CLOCK_PANEL_Y + 14, time_buf, 1, mix_color(muted, accent, 1, 2));
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
    const int cx = CLOCK_PANEL_X + 92;
    const int cy = CLOCK_PANEL_Y + 118;
    const int radius = 56;
    const uint16_t face_top = rgb565(22, 34, 70);
    const uint16_t face_bottom = rgb565(10, 16, 38);
    const uint16_t ring = rgb565(76, 176, 255);
    const uint16_t ring_dim = rgb565(28, 52, 104);
    const uint16_t tick = rgb565(64, 82, 130);
    const uint16_t tick_major = rgb565(176, 214, 255);
    const uint16_t number = rgb565(214, 226, 248);
    const uint16_t hour_color = rgb565(240, 246, 255);
    const uint16_t minute_color = rgb565(120, 200, 255);
    const uint16_t second_color = rgb565(255, 96, 150);
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(142, 158, 191);
    char hm_buf[8];
    char sec_buf[8];
    char date_buf[32];

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(6, 10, 28), rgb565(8, 16, 36)),
                        TAG, "draw analog bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("时间", tm, rgb565(76, 176, 255)), TAG, "draw analog header failed");

    // 表盘：外圈光晕 + 主题色细环 + 上亮下暗的深色表面。
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, radius + 6, rgb565(12, 22, 48)),
                        TAG, "draw clock halo failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, radius + 3, ring_dim), TAG, "draw clock ring failed");
    ESP_RETURN_ON_ERROR(lcd_draw_arc(cx, cy, radius + 2, -150, -30, 2, ring), TAG, "draw clock ring hi failed");
    for (int y = -radius; y <= radius; ++y) {
        int x_span = (int)sqrtf((float)(radius * radius - y * y));
        ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - x_span, cy + y, x_span * 2 + 1, 1,
                                          mix_color(face_top, face_bottom, y + radius, radius * 2)),
                            TAG, "draw clock face failed");
    }

    for (int i = 0; i < 60; ++i) {
        float degrees = (float)i * 6.0f - 90.0f;
        float rad = degrees * (float)M_PI / 180.0f;
        bool major = (i % 5) == 0;
        int outer = radius - 3;
        int inner = radius - (major ? 9 : 5);
        int x0 = cx + (int)roundf(cosf(rad) * inner);
        int y0 = cy + (int)roundf(sinf(rad) * inner);
        int x1 = cx + (int)roundf(cosf(rad) * outer);
        int y1 = cy + (int)roundf(sinf(rad) * outer);
        if (major) {
            ESP_RETURN_ON_ERROR(lcd_draw_line(x0, y0, x1, y1, 2, (i % 15) == 0 ? ring : tick_major),
                                TAG, "draw clock tick failed");
        } else {
            ESP_RETURN_ON_ERROR(lcd_fill_rect(x1, y1, 1, 1, tick), TAG, "draw clock tick failed");
        }
    }

    ESP_RETURN_ON_ERROR(draw_text5_centered(cx, cy - radius + 13, "12", 1, number),
                        TAG, "draw clock number failed");
    ESP_RETURN_ON_ERROR(draw_text5_centered(cx + radius - 16, cy - 3, "3", 1, number),
                        TAG, "draw clock number failed");
    ESP_RETURN_ON_ERROR(draw_text5_centered(cx, cy + radius - 20, "6", 1, number),
                        TAG, "draw clock number failed");
    ESP_RETURN_ON_ERROR(draw_text5_centered(cx - radius + 16, cy - 3, "9", 1, number),
                        TAG, "draw clock number failed");

    float hour_degrees = ((float)(tm->tm_hour % 12) + (float)tm->tm_min / 60.0f +
                          (float)tm->tm_sec / 3600.0f) * 30.0f - 90.0f;
    float minute_degrees = ((float)tm->tm_min + (float)tm->tm_sec / 60.0f) * 6.0f - 90.0f;
    float second_degrees = (float)tm->tm_sec * 6.0f - 90.0f;
    ESP_RETURN_ON_ERROR(draw_clock_hand(cx + 1, cy + 2, hour_degrees, 28, 5, rgb565(4, 8, 20)),
                        TAG, "draw hour shadow failed");
    ESP_RETURN_ON_ERROR(draw_clock_hand(cx, cy, hour_degrees, 28, 5, hour_color),
                        TAG, "draw hour hand failed");
    ESP_RETURN_ON_ERROR(draw_clock_hand(cx + 1, cy + 2, minute_degrees, 42, 3, rgb565(4, 8, 20)),
                        TAG, "draw minute shadow failed");
    ESP_RETURN_ON_ERROR(draw_clock_hand(cx, cy, minute_degrees, 42, 3, minute_color),
                        TAG, "draw minute hand failed");
    ESP_RETURN_ON_ERROR(draw_clock_hand(cx, cy, second_degrees + 180.0f, 10, 1, second_color),
                        TAG, "draw second tail failed");
    ESP_RETURN_ON_ERROR(draw_clock_hand(cx, cy, second_degrees, 48, 1, second_color),
                        TAG, "draw second hand failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 4, hour_color), TAG, "draw clock center failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 2, second_color), TAG, "draw clock pin failed");

    // 右侧数字信息栏。
    const int info_x = CLOCK_PANEL_X + 170;
    const int info_w = CLOCK_PANEL_W - 170 - 14;
    const int info_cx = info_x + info_w / 2;
    snprintf(hm_buf, sizeof(hm_buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
    snprintf(sec_buf, sizeof(sec_buf), "%02d", tm->tm_sec);
    snprintf(date_buf, sizeof(date_buf), "%04d/%02d/%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    ESP_RETURN_ON_ERROR(draw_text5_centered(info_cx, CLOCK_PANEL_Y + 62, hm_buf, 3, text),
                        TAG, "draw analog digital failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(info_cx - 22, CLOCK_PANEL_Y + 92, 44, 22, 11,
                                            mix_color(rgb565(10, 16, 36), second_color, 1, 5)),
                        TAG, "draw seconds chip failed");
    ESP_RETURN_ON_ERROR(draw_text5_centered(info_cx, CLOCK_PANEL_Y + 96, sec_buf, 2, second_color),
                        TAG, "draw analog seconds failed");
    ESP_RETURN_ON_ERROR(lcd_draw_hline(info_x + 8, CLOCK_PANEL_Y + 126, info_w - 16, rgb565(32, 46, 84)),
                        TAG, "draw analog divider failed");
    ESP_RETURN_ON_ERROR(draw_text5_centered(info_cx, CLOCK_PANEL_Y + 138, date_buf, 1, muted),
                        TAG, "draw analog date failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_centered_limited(info_cx, CLOCK_PANEL_Y + 154,
                                                         weekday_name(tm->tm_wday), info_w, 1,
                                                         rgb565(176, 150, 255)),
                        TAG, "draw analog weekday failed");
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
    const int cell_w = 38;
    const int grid_x = 160 - cell_w * 7 / 2;
    const int week_y = CLOCK_PANEL_Y + 42;
    const int day_y = CLOCK_PANEL_Y + 66;
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(140, 156, 190);
    const uint16_t faint = rgb565(56, 66, 100);
    const uint16_t weekend = rgb565(121, 206, 255);
    const uint16_t today_bg = rgb565(196, 110, 255);
    const uint16_t today_text = rgb565(255, 255, 255);
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
    int prev_days = days_in_month(month == 1 ? year - 1 : year, month == 1 ? 12 : month - 1);
    int rows = (first_wday + month_days + 6) / 7;
    int row_h = rows > 5 ? 22 : 26;

    snprintf(month_buf, sizeof(month_buf), "%04d/%02d", year, month);
    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(8, 10, 28), rgb565(18, 14, 38)),
                        TAG, "draw calendar bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header(month_buf, tm, rgb565(196, 110, 255)), TAG,
                        "draw calendar header failed");

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(grid_x - 4, week_y - 2, cell_w * 7 + 8, 20, 6, rgb565(20, 22, 50)),
                        TAG, "draw week band failed");
    for (int col = 0; col < 7; ++col) {
        int center_x = grid_x + col * cell_w + cell_w / 2;
        ESP_RETURN_ON_ERROR(draw_text_mixed_centered_limited(center_x, week_y, week_labels[col], 18, 1,
                                                             (col == 0 || col == 6) ? weekend : muted),
                            TAG, "draw weekday label failed");
    }

    for (int index = 0; index < rows * 7; ++index) {
        int col = index % 7;
        int row = index / 7;
        int day = index - first_wday + 1;
        int center_x = grid_x + col * cell_w + cell_w / 2;
        int y = day_y + row * row_h;
        char day_buf[12];
        uint16_t day_color = text;

        if (day < 1) {
            snprintf(day_buf, sizeof(day_buf), "%d", prev_days + day);
            day_color = faint;
        } else if (day > month_days) {
            snprintf(day_buf, sizeof(day_buf), "%d", day - month_days);
            day_color = faint;
        } else {
            snprintf(day_buf, sizeof(day_buf), "%d", day);
            if (day == tm->tm_mday) {
                ESP_RETURN_ON_ERROR(lcd_fill_round_rect(center_x - 15, y - 3, 30, 19, 9,
                                                        mix_color(today_bg, rgb565(10, 10, 30), 1, 2)),
                                    TAG, "draw today glow failed");
                ESP_RETURN_ON_ERROR(lcd_fill_round_rect(center_x - 13, y - 2, 26, 17, 8, today_bg),
                                    TAG, "draw today highlight failed");
                day_color = today_text;
            } else if (col == 0 || col == 6) {
                day_color = weekend;
            }
        }
        ESP_RETURN_ON_ERROR(draw_text5_centered(center_x, y + 3, day_buf, 1, day_color),
                            TAG, "draw calendar day failed");
    }

    return lcd_canvas_flush();
}

static esp_err_t draw_cloud_symbol(int cx, int cy, uint16_t cloud, uint16_t shade)
{
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 11, cy + 3, 10, shade), TAG, "draw cloud shade failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy - 2, 13, cloud), TAG, "draw cloud body failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx + 13, cy + 4, 9, cloud), TAG, "draw cloud body failed");
    return lcd_fill_round_rect(cx - 21, cy + 3, 43, 14, 6, cloud);
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
        return draw_sun_icon(cx, cy, 13, sun);
    }

    if (cloudy || foggy) {
        ESP_RETURN_ON_ERROR(draw_sun_icon(cx - 14, cy - 7, 8, sun), TAG, "draw cloud sun failed");
    }
    ESP_RETURN_ON_ERROR(draw_cloud_symbol(cx, cy, cloud, shade), TAG, "draw cloud failed");

    if (thunder) {
        return lcd_fill_triangle(cx - 1, cy + 12, cx - 9, cy + 30, cx + 5, cy + 18,
                                 rgb565(255, 216, 73));
    }
    if (rainy) {
        for (int i = 0; i < 4; ++i) {
            int x = cx - 14 + i * 10;
            ESP_RETURN_ON_ERROR(lcd_draw_line(x, cy + 20, x - 3, cy + 28, 2, rain),
                                TAG, "draw rain failed");
        }
        return ESP_OK;
    }
    if (snowy) {
        for (int i = 0; i < 4; ++i) {
            ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 15 + i * 10, cy + 22 + (i % 2) * 4,
                                                2, snow),
                                TAG, "draw snow failed");
        }
    }
    if (foggy) {
        ESP_RETURN_ON_ERROR(lcd_draw_hline(cx - 21, cy + 21, 43, shade), TAG, "draw fog failed");
        return lcd_draw_hline(cx - 16, cy + 26, 32, shade);
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

static uint16_t temperature_color(int temp_c)
{
    // 冷色到暖色：<=0 蓝，10 青，20 绿黄，>=30 橙。
    if (temp_c <= 0) {
        return rgb565(80, 150, 255);
    }
    if (temp_c <= 10) {
        return mix_color(rgb565(80, 150, 255), rgb565(76, 226, 230), temp_c, 10);
    }
    if (temp_c <= 20) {
        return mix_color(rgb565(76, 226, 230), rgb565(190, 230, 110), temp_c - 10, 10);
    }
    if (temp_c <= 30) {
        return mix_color(rgb565(190, 230, 110), rgb565(255, 160, 80), temp_c - 20, 10);
    }
    return rgb565(255, 120, 80);
}

static esp_err_t draw_weather_forecast_rows(const struct tm *tm, const weather_info_t *weather)
{
    const uint16_t text = rgb565(220, 238, 248);
    const uint16_t muted = rgb565(133, 158, 181);
    const uint16_t track = rgb565(24, 40, 62);
    const uint16_t today_bg = rgb565(16, 36, 52);
    const int row_x = CLOCK_PANEL_X + 14;
    const int row_w = CLOCK_PANEL_W - 28;
    const int row_y0 = CLOCK_PANEL_Y + 105;
    const int row_h = 13;
    const int icon_x = row_x + 48;
    const int min_right = row_x + 94;
    const int bar_x = row_x + 102;
    const int bar_w = row_w - 102 - 34;
    const int max_x = bar_x + bar_w + 8;
    int range_min = 1000;
    int range_max = -1000;

    for (int i = 0; weather != NULL && i < WEATHER_FORECAST_DAYS && i < weather->forecast_count; ++i) {
        if (!weather->forecast[i].valid) {
            continue;
        }
        if (weather->forecast[i].min_c < range_min) {
            range_min = weather->forecast[i].min_c;
        }
        if (weather->forecast[i].max_c > range_max) {
            range_max = weather->forecast[i].max_c;
        }
    }
    if (range_max <= range_min) {
        range_max = range_min + 1;
    }

    for (int i = 0; i < WEATHER_FORECAST_DAYS; ++i) {
        const bool valid = weather != NULL && i < weather->forecast_count && weather->forecast[i].valid;
        const char *condition = valid ? weather->forecast[i].condition : NULL;
        char label[12];
        char min_buf[12];
        char max_buf[12];
        int y = row_y0 + i * row_h;

        if (i == 0 && valid) {
            ESP_RETURN_ON_ERROR(lcd_fill_round_rect(row_x - 4, y - 3, row_w + 8, row_h, 4, today_bg),
                                TAG, "draw forecast today bg failed");
        }
        if (!valid) {
            ESP_RETURN_ON_ERROR(draw_text5(row_x, y, "--/--", 1, muted), TAG, "draw forecast label failed");
            ESP_RETURN_ON_ERROR(lcd_fill_round_rect(bar_x, y + 2, bar_w, 3, 1, track),
                                TAG, "draw forecast track failed");
            continue;
        }

        weather_forecast_label(weather->forecast[i].date, tm, label, sizeof(label));
        snprintf(min_buf, sizeof(min_buf), "%d" DEG_SIGN, weather->forecast[i].min_c);
        snprintf(max_buf, sizeof(max_buf), "%d" DEG_SIGN, weather->forecast[i].max_c);

        ESP_RETURN_ON_ERROR(draw_text5(row_x, y, label, 1, i == 0 ? rgb565(95, 236, 221) : text),
                            TAG, "draw forecast label failed");
        ESP_RETURN_ON_ERROR(draw_weather_mini_symbol(condition, icon_x, y + 1, muted),
                            TAG, "draw forecast mini failed");
        ESP_RETURN_ON_ERROR(draw_text5(min_right - text5_width(min_buf, 1), y, min_buf, 1, muted),
                            TAG, "draw forecast min failed");

        // 温度区间条：灰色轨道表示 7 日总区间，彩色段表示当天最低到最高温。
        int seg_x0 = bar_x + ((weather->forecast[i].min_c - range_min) * (bar_w - 4)) / (range_max - range_min);
        int seg_x1 = bar_x + 4 + ((weather->forecast[i].max_c - range_min) * (bar_w - 4)) / (range_max - range_min);
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(bar_x, y + 2, bar_w, 3, 1, track),
                            TAG, "draw forecast track failed");
        uint16_t c0 = temperature_color(weather->forecast[i].min_c);
        uint16_t c1 = temperature_color(weather->forecast[i].max_c);
        int seg_w = seg_x1 - seg_x0;
        for (int px = 0; px < seg_w; ++px) {
            int h = (px == 0 || px == seg_w - 1) ? 3 : 5;
            ESP_RETURN_ON_ERROR(lcd_fill_rect(seg_x0 + px, y + 1 + (5 - h) / 2, 1, h,
                                              mix_color(c0, c1, px, seg_w > 1 ? seg_w - 1 : 1)),
                                TAG, "draw forecast range failed");
        }
        ESP_RETURN_ON_ERROR(draw_text5(max_x, y, max_buf, 1, text), TAG, "draw forecast max failed");
    }
    return ESP_OK;
}

static esp_err_t draw_weather_screen(const struct tm *tm)
{
    weather_info_t weather = {0};
    const char *condition_text = "等待";
    const char *city_text = CONFIG_CLOCK_WEATHER_CITY_LABEL;
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(145, 161, 196);
    const uint16_t accent = rgb565(95, 236, 221);
    const uint16_t card = rgb565(10, 30, 46);
    const uint16_t card_border = rgb565(26, 58, 76);
    char temp_buf[16];
    char hum_buf[16];
    char updated_buf[16];

    weather_get_snapshot(&weather);
    if (weather.valid) {
        snprintf(temp_buf, sizeof(temp_buf), "%d" DEG_SIGN, weather.temp_c);
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
        strlcpy(temp_buf, "--" DEG_SIGN, sizeof(temp_buf));
        strlcpy(hum_buf, "--%", sizeof(hum_buf));
        strlcpy(updated_buf, "UP --:--", sizeof(updated_buf));
        condition_text = s_prov_active ? "配网" : "等待";
    }

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(5, 12, 28), rgb565(5, 24, 34)),
                        TAG, "draw weather bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("天气", tm, accent), TAG, "draw weather header failed");

    // 当前天气卡片。
    const int card_x = CLOCK_PANEL_X + 10;
    const int card_y = CLOCK_PANEL_Y + 42;
    const int card_w = CLOCK_PANEL_W - 20;
    const int card_h = 56;
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(card_x, card_y, card_w, card_h, 8, card_border),
                        TAG, "draw weather card border failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(card_x + 1, card_y + 1, card_w - 2, card_h - 2, 7, card),
                        TAG, "draw weather card failed");
    ESP_RETURN_ON_ERROR(draw_weather_symbol(weather.valid ? weather.condition : NULL,
                                            card_x + 40, card_y + 25),
                        TAG, "draw weather symbol failed");
    ESP_RETURN_ON_ERROR(draw_text5(card_x + 82, card_y + 14, temp_buf, 4,
                                   weather.valid ? temperature_color(weather.temp_c) : muted),
                        TAG, "draw weather temp failed");

    const int info_x = card_x + 172;
    const int info_w = card_w - 172 - 8;
    ESP_RETURN_ON_ERROR(draw_location_pin(info_x + 3, card_y + 9, rgb565(255, 120, 170), card),
                        TAG, "draw weather pin failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(info_x + 11, card_y + 4, city_text, info_w - 11, 1, muted),
                        TAG, "draw weather city failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(info_x, card_y + 20, condition_text, 40, 1, text),
                        TAG, "draw weather condition failed");
    ESP_RETURN_ON_ERROR(draw_text5(info_x + 46, card_y + 25, hum_buf, 1, accent),
                        TAG, "draw humidity value failed");
    ESP_RETURN_ON_ERROR(draw_text5(info_x, card_y + 42, updated_buf, 1, rgb565(96, 120, 150)),
                        TAG, "draw weather update failed");
    ESP_RETURN_ON_ERROR(draw_weather_forecast_rows(tm, &weather), TAG, "draw forecast failed");
    return lcd_canvas_flush();
}

static bool today_hour_is_current(const char *time_text)
{
    int hour = -1;
    int minute = -1;
    if (time_text == NULL ||
        sscanf(time_text, "%d:%d", &hour, &minute) != 2 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59) {
        return false;
    }

    time_t now = 0;
    time(&now);
    struct tm now_tm = {0};
    localtime_r(&now, &now_tm);
    return hour == now_tm.tm_hour;
}

static esp_err_t draw_today_hourly_card(int x, int y, int w, int h,
                                        const char *time_text,
                                        const char *condition,
                                        int temp_c,
                                        int humidity,
                                        bool current)
{
    const uint16_t text = rgb565(222, 238, 248);
    const uint16_t muted = rgb565(126, 146, 174);
    const uint16_t accent = current ? rgb565(255, 211, 85) : rgb565(76, 226, 255);
    const uint16_t border = current ? rgb565(255, 211, 85) : rgb565(31, 48, 68);
    const uint16_t card = current ? rgb565(44, 39, 33) : rgb565(8, 15, 31);
    char temp_buf[12];
    char hum_buf[12];
    int temp_x = 0;

    snprintf(temp_buf, sizeof(temp_buf), "%d" DEG_SIGN, temp_c);
    snprintf(hum_buf, sizeof(hum_buf), "RH%d%%", humidity);
    temp_x = x + w - text5_width(temp_buf, 2) - 9;

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, w, h, 5, border),
                        TAG, "draw hourly card border failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 1, y + 1, w - 2, h - 2, 4, card),
                        TAG, "draw hourly card failed");
    if (current) {
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 3, y + 3, 3, h - 6, 2, accent),
                            TAG, "draw hourly current mark failed");
    }
    ESP_RETURN_ON_ERROR(draw_text5(x + (current ? 11 : 8), y + 5,
                                   time_text != NULL ? time_text : "--:--", 1,
                                   current ? accent : text),
                        TAG, "draw hourly card time failed");
    ESP_RETURN_ON_ERROR(draw_text5(x + (current ? 11 : 8), y + h - 10, hum_buf, 1, muted),
                        TAG, "draw hourly card humidity failed");
    ESP_RETURN_ON_ERROR(draw_weather_mini_symbol(condition, x + 61, y + h / 2, accent),
                        TAG, "draw hourly card mini failed");
    return draw_text5(temp_x, y + 7, temp_buf, 2, accent);
}

static esp_err_t draw_today_page_indicator(int page)
{
    const uint16_t muted = rgb565(133, 158, 181);
    const uint16_t active = rgb565(255, 211, 85);
    char page_buf[8];

    if (page < 0) {
        page = 0;
    } else if (page >= TODAY_HOURLY_PAGE_COUNT) {
        page = TODAY_HOURLY_PAGE_COUNT - 1;
    }

    snprintf(page_buf, sizeof(page_buf), "%d/%d", page + 1, TODAY_HOURLY_PAGE_COUNT);
    ESP_RETURN_ON_ERROR(draw_text5(CLOCK_PANEL_X + CLOCK_PANEL_W - text5_width(page_buf, 1) - 14,
                                   CLOCK_PANEL_Y + 8, page_buf, 1, active),
                        TAG, "draw today page text failed");
    for (int i = 0; i < TODAY_HOURLY_PAGE_COUNT; ++i) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(CLOCK_PANEL_X + CLOCK_PANEL_W - 48 + i * 10,
                                            CLOCK_PANEL_Y + 12, 2,
                                            i == page ? active : muted),
                            TAG, "draw today page dot failed");
    }
    return ESP_OK;
}

static esp_err_t draw_today_hourly_rows(const weather_info_t *weather, int page)
{
    const uint16_t muted = rgb565(133, 158, 181);
    const int row_x = CLOCK_PANEL_X + 10;
    const int col_gap = 8;
    const int col_w = (CLOCK_PANEL_W - 20 - col_gap) / 2;
    const int row_y0 = CLOCK_PANEL_Y + 16;
    const int row_h = 31;
    const int card_h = 29;
    const int rows_per_col = 6;
    int page_offset = 0;

    if (page < 0) {
        page = 0;
    } else if (page >= TODAY_HOURLY_PAGE_COUNT) {
        page = TODAY_HOURLY_PAGE_COUNT - 1;
    }
    page_offset = page * TODAY_HOURLY_ITEMS_PER_PAGE;

    if (weather == NULL || weather->hourly_count <= 0) {
        ESP_RETURN_ON_ERROR(draw_text5_centered(160, CLOCK_PANEL_Y + 103, "WAIT", 2, muted),
                            TAG, "draw hourly empty failed");
        return ESP_OK;
    }

    for (int i = 0; i < TODAY_HOURLY_ITEMS_PER_PAGE; ++i) {
        int idx = page_offset + i;
        const bool valid = idx < weather->hourly_count && weather->hourly[idx].valid;
        const char *condition = valid ? weather->hourly[idx].condition : NULL;
        int col = i / rows_per_col;
        int row = i % rows_per_col;
        int x = row_x + col * (col_w + col_gap);
        int y = row_y0 + row * row_h;

        if (!valid || idx >= WEATHER_HOURLY_SLOTS || col >= 2) {
            continue;
        }

        ESP_RETURN_ON_ERROR(draw_today_hourly_card(x, y, col_w, card_h,
                                                   weather->hourly[idx].time,
                                                   condition,
                                                   weather->hourly[idx].temp_c,
                                                   weather->hourly[idx].humidity,
                                                   today_hour_is_current(weather->hourly[idx].time)),
                            TAG, "draw hourly card failed");
    }
    return ESP_OK;
}

static esp_err_t draw_today_weather_screen(const struct tm *tm)
{
    weather_info_t weather = {0};
    (void)tm;

    weather_get_snapshot(&weather);

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(6, 9, 24), rgb565(18, 20, 34)),
                        TAG, "draw today bg failed");

    ESP_RETURN_ON_ERROR(draw_today_page_indicator(s_today_hourly_page),
                        TAG, "draw today page indicator failed");
    ESP_RETURN_ON_ERROR(draw_today_hourly_rows(&weather, s_today_hourly_page),
                        TAG, "draw today hourly rows failed");
    return lcd_canvas_flush();
}

static esp_err_t draw_komari_detail_row(int y, const char *label, const char *value,
                                        uint16_t label_color, uint16_t value_color)
{
    const uint16_t line = rgb565(26, 30, 50);
    const int x = CLOCK_PANEL_X + 18;
    const int value_x = CLOCK_PANEL_X + 80;

    ESP_RETURN_ON_ERROR(draw_text5(x, y + 1, label, 1, label_color), TAG, "draw komari label failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(value_x, y, value != NULL ? value : "--",
                                                CLOCK_PANEL_X + CLOCK_PANEL_W - value_x - 16,
                                                1, value_color),
                        TAG, "draw komari value failed");
    return lcd_draw_hline(x, y + 13, CLOCK_PANEL_W - 36, line);
}

static esp_err_t draw_komari_screen(const struct tm *tm)
{
    komari_info_t komari = {0};
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(145, 161, 196);
    const uint16_t accent = rgb565(255, 185, 85);
    const uint16_t band_top = rgb565(30, 22, 22);
    const uint16_t band_bottom = rgb565(84, 60, 36);
    char online_buf[24];
    char nodes_buf[16];
    char private_buf[8];
    char record_buf[8];
    char updated_buf[16];

    komari_get_snapshot(&komari);
    if (!komari.configured) {
        strlcpy(online_buf, "NOT SET", sizeof(online_buf));
        strlcpy(nodes_buf, "--", sizeof(nodes_buf));
        strlcpy(private_buf, "--", sizeof(private_buf));
        strlcpy(record_buf, "--", sizeof(record_buf));
        strlcpy(updated_buf, "UP --:--", sizeof(updated_buf));
    } else {
        if (!komari.valid) {
            strlcpy(online_buf, "WAIT", sizeof(online_buf));
        } else if (komari.online_valid && komari.node_count >= 0) {
            snprintf(online_buf, sizeof(online_buf), "%d/%d", komari.online_count, komari.node_count);
        } else if (komari.online_valid) {
            snprintf(online_buf, sizeof(online_buf), "%d", komari.online_count);
        } else if (komari.nodes_valid) {
            snprintf(online_buf, sizeof(online_buf), "--/%d", komari.node_count);
        } else {
            strlcpy(online_buf, "ERR", sizeof(online_buf));
        }

        if (komari.nodes_valid && komari.node_count >= 0) {
            snprintf(nodes_buf, sizeof(nodes_buf), "%d", komari.node_count);
        } else {
            strlcpy(nodes_buf, "--", sizeof(nodes_buf));
        }
        strlcpy(private_buf, komari.public_valid ? (komari.private_site ? "YES" : "NO") : "--",
                sizeof(private_buf));
        strlcpy(record_buf, komari.public_valid ? (komari.record_enabled ? "YES" : "NO") : "--",
                sizeof(record_buf));
        if (komari.updated_at > 0) {
            struct tm updated_tm = {0};
            localtime_r(&komari.updated_at, &updated_tm);
            snprintf(updated_buf, sizeof(updated_buf), "UP %02d:%02d",
                     updated_tm.tm_hour, updated_tm.tm_min);
        } else {
            strlcpy(updated_buf, "UP --:--", sizeof(updated_buf));
        }
    }

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(8, 9, 24), rgb565(20, 16, 28)),
                        TAG, "draw komari bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("KOMARI", tm, accent), TAG, "draw komari header failed");

    // 在线概览卡片：左侧标签和进度条，右侧大号在线/总数。
    const int card_x = CLOCK_PANEL_X + 10;
    const int card_y = CLOCK_PANEL_Y + 42;
    const int card_w = CLOCK_PANEL_W - 20;
    const int card_h = 38;
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(card_x, card_y, card_w, card_h, 8, band_bottom),
                        TAG, "draw komari card border failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(card_x + 1, card_y + 1, card_w - 2, card_h - 2, 7, band_top),
                        TAG, "draw komari card failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(card_x + 14, card_y + 13, 3,
                                        komari.online_valid && komari.online_count > 0 ?
                                        rgb565(76, 235, 142) : muted),
                        TAG, "draw komari online dot failed");
    ESP_RETURN_ON_ERROR(draw_text5(card_x + 24, card_y + 10, "ONLINE", 1, muted),
                        TAG, "draw komari online label failed");
    const int bar_x = card_x + 12;
    const int bar_w = 120;
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(bar_x, card_y + 24, bar_w, 5, 2, rgb565(56, 42, 30)),
                        TAG, "draw komari bar track failed");
    if (komari.online_valid && komari.node_count > 0) {
        int fill = (bar_w * komari.online_count) / komari.node_count;
        if (fill > bar_w) {
            fill = bar_w;
        }
        if (fill > 4) {
            ESP_RETURN_ON_ERROR(lcd_fill_round_rect(bar_x, card_y + 24, fill, 5, 2, accent),
                                TAG, "draw komari bar fill failed");
        }
    }
    ESP_RETURN_ON_ERROR(draw_text5(card_x + card_w - text5_width(online_buf, 3) - 12,
                                   card_y + 9, online_buf, 3, accent),
                        TAG, "draw komari online failed");

    ESP_RETURN_ON_ERROR(draw_komari_detail_row(CLOCK_PANEL_Y + 86, "SITE",
                                               komari.configured ? komari.sitename : "NOT SET",
                                               accent, text),
                        TAG, "draw komari site failed");
    ESP_RETURN_ON_ERROR(draw_komari_detail_row(CLOCK_PANEL_Y + 100, "DESC",
                                               komari.configured ? komari.description : "--",
                                               muted, text),
                        TAG, "draw komari desc failed");
    ESP_RETURN_ON_ERROR(draw_komari_detail_row(CLOCK_PANEL_Y + 114, "NODES",
                                               nodes_buf, muted, text),
                        TAG, "draw komari nodes failed");
    ESP_RETURN_ON_ERROR(draw_komari_detail_row(CLOCK_PANEL_Y + 128, "THEME",
                                               komari.configured ? komari.theme : "--",
                                               muted, text),
                        TAG, "draw komari theme failed");
    ESP_RETURN_ON_ERROR(draw_komari_detail_row(CLOCK_PANEL_Y + 142, "VER",
                                               komari.configured ? komari.version : "--",
                                               muted, text),
                        TAG, "draw komari version failed");
    ESP_RETURN_ON_ERROR(draw_komari_detail_row(CLOCK_PANEL_Y + 156, "PRIVATE",
                                               private_buf, muted, text),
                        TAG, "draw komari private failed");
    ESP_RETURN_ON_ERROR(draw_komari_detail_row(CLOCK_PANEL_Y + 170, "RECORD",
                                               record_buf, muted, text),
                        TAG, "draw komari record failed");
    ESP_RETURN_ON_ERROR(draw_komari_detail_row(CLOCK_PANEL_Y + 184, "UP",
                                               updated_buf, muted, text),
                        TAG, "draw komari update failed");
    return lcd_canvas_flush();
}

static esp_err_t draw_dialog_box(int x, int y, int w, int h, uint16_t accent)
{
    const uint16_t border = mix_color(rgb565(40, 56, 96), accent, 1, 3);
    const uint16_t top = rgb565(18, 26, 54);
    const uint16_t bottom = rgb565(8, 13, 32);

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 2, y + 4, w, h, 9, rgb565(2, 4, 12)),
                        TAG, "draw dialog shadow failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, w, h, 9, border), TAG, "draw dialog border failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 1, y + 1, w - 2, h - 2, 8, bottom),
                        TAG, "draw dialog body failed");
    for (int row = 0; row < h / 2; ++row) {
        int inset = row < 8 ? 8 - (int)sqrtf((float)(64 - (8 - row) * (8 - row))) : 0;
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + 1 + inset, y + 1 + row, w - 2 - inset * 2, 1,
                                          mix_color(top, bottom, row, h / 2)),
                            TAG, "draw dialog gradient failed");
    }
    return lcd_fill_round_rect(x + w / 2 - 18, y, 36, 2, 1, accent);
}

static esp_err_t draw_dialog_button(int x, int y, int w, int h, const char *label,
                                    bool primary, uint16_t accent)
{
    const uint16_t dark = rgb565(15, 20, 30);
    if (primary) {
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, w, h, h / 2, accent), TAG, "draw button failed");
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + h / 2, y + 1, w - h, 1, mix_color(accent, rgb565(255, 255, 255), 1, 2)),
                            TAG, "draw button highlight failed");
    } else {
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, w, h, h / 2, mix_color(rgb565(30, 40, 70), accent, 1, 3)),
                            TAG, "draw button border failed");
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 1, y + 1, w - 2, h - 2, h / 2 - 1, rgb565(16, 22, 44)),
                            TAG, "draw button fill failed");
    }
    int text_w = text_mixed_width(label, 1);
    bool ascii = label[0] != '\0' && ((unsigned char)label[0] & 0x80) == 0;
    int text_y = ascii ? y + (h - 7) / 2 - 4 : y + (h - 16) / 2;
    return draw_text_mixed_limited(x + (w - text_w) / 2, text_y, label, w - 8, 1,
                                   primary ? dark : accent);
}

static esp_err_t draw_wifi_reprovision_confirm_screen(const struct tm *tm)
{
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(145, 161, 196);
    const uint16_t accent = rgb565(255, 211, 85);
    const uint16_t cyan = rgb565(64, 235, 255);

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(5, 8, 22), rgb565(12, 18, 34)),
                        TAG, "draw wifi confirm bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("WIFI", tm, cyan), TAG, "draw wifi confirm header failed");

    ESP_RETURN_ON_ERROR(draw_dialog_box(WIFI_CONFIRM_BOX_X, WIFI_CONFIRM_BOX_Y,
                                        WIFI_CONFIRM_BOX_W, WIFI_CONFIRM_BOX_H, cyan),
                        TAG, "draw wifi confirm box failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(WIFI_CONFIRM_BOX_X + 30, WIFI_CONFIRM_BOX_Y + 28, 16,
                                        mix_color(rgb565(12, 20, 44), cyan, 1, 5)),
                        TAG, "draw wifi confirm icon bg failed");
    ESP_RETURN_ON_ERROR(draw_wifi_icon(WIFI_CONFIRM_BOX_X + 18, WIFI_CONFIRM_BOX_Y + 19, true),
                        TAG, "draw wifi confirm icon failed");
    ESP_RETURN_ON_ERROR(draw_text5(WIFI_CONFIRM_BOX_X + 56, WIFI_CONFIRM_BOX_Y + 17,
                                   "SETUP WIFI?", 1, text),
                        TAG, "draw wifi confirm title failed");
    ESP_RETURN_ON_ERROR(draw_text5(WIFI_CONFIRM_BOX_X + 56, WIFI_CONFIRM_BOX_Y + 33,
                                   "OPEN AP MODE", 1, muted),
                        TAG, "draw wifi confirm body failed");

    ESP_RETURN_ON_ERROR(draw_dialog_button(WIFI_CONFIRM_YES_X, WIFI_CONFIRM_YES_Y,
                                           WIFI_CONFIRM_BUTTON_W, WIFI_CONFIRM_BUTTON_H,
                                           "YES", true, accent),
                        TAG, "draw wifi yes button failed");
    ESP_RETURN_ON_ERROR(draw_dialog_button(WIFI_CONFIRM_NO_X, WIFI_CONFIRM_NO_Y,
                                           WIFI_CONFIRM_BUTTON_W, WIFI_CONFIRM_BUTTON_H,
                                           "NO", false, accent),
                        TAG, "draw wifi no button failed");
    return lcd_canvas_flush();
}

static void format_uptime(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }

    time_t now = time(NULL);
    long seconds = 0;
    if (s_boot_time > 0 && now >= s_boot_time) {
        seconds = (long)difftime(now, s_boot_time);
    }

    long days = seconds / 86400;
    long hours = (seconds % 86400) / 3600;
    long minutes = (seconds % 3600) / 60;
    long secs = seconds % 60;
    if (days > 0) {
        snprintf(out, out_len, "%ldD %02ld:%02ld", days, hours, minutes);
    } else {
        snprintf(out, out_len, "%02ld:%02ld:%02ld", hours, minutes, secs);
    }
}

static void format_time_age(time_t timestamp, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    if (timestamp <= 0) {
        strlcpy(out, "--", out_len);
        return;
    }

    time_t now = time(NULL);
    long seconds = now >= timestamp ? (long)difftime(now, timestamp) : 0;
    if (seconds < 60) {
        snprintf(out, out_len, "%ld秒前", seconds);
    } else if (seconds < 3600) {
        snprintf(out, out_len, "%ld分前", seconds / 60);
    } else if (seconds < 86400) {
        snprintf(out, out_len, "%ld时前", seconds / 3600);
    } else {
        snprintf(out, out_len, "%ld天前", seconds / 86400);
    }
}

static void format_partition_label(const esp_partition_t *partition, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    if (partition == NULL) {
        strlcpy(out, "--", out_len);
        return;
    }

    size_t out_pos = 0;
    for (size_t i = 0; partition->label[i] != '\0' && out_pos + 1 < out_len; ++i) {
        char ch = partition->label[i];
        out[out_pos++] = ch == '_' ? '-' : (char)toupper((unsigned char)ch);
    }
    out[out_pos] = '\0';
}

static esp_err_t draw_health_row(int x, int y, int w, const char *label, const char *value,
                                 uint16_t label_color, uint16_t value_color)
{
    const uint16_t line = rgb565(24, 34, 60);
    const int label_w = 38;
    const int value_x = x + 44;
    int value_w = w - (value_x - x);
    if (value_w < 0) {
        value_w = 0;
    }

    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(x, y, label, label_w, 1, label_color),
                        TAG, "draw health label failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(value_x, y, value != NULL ? value : "--",
                                                value_w, 1, value_color),
                        TAG, "draw health value failed");
    return lcd_draw_hline(x, y + 16, w, line);
}

static esp_err_t draw_health_screen(const struct tm *tm)
{
    (void)tm;
    const uint16_t text = rgb565(230, 241, 255);
    const uint16_t muted = rgb565(135, 153, 188);
    const uint16_t accent = rgb565(94, 236, 218);
    const uint16_t warning = rgb565(255, 211, 85);
    const uint16_t button_bg = rgb565(255, 196, 76);
    const int full_x = CLOCK_PANEL_X + 16;
    const int full_w = CLOCK_PANEL_W - 32;
    const int full_y0 = CLOCK_PANEL_Y + 46;
    const int full_row_h = 18;
    const int col_y0 = CLOCK_PANEL_Y + 84;
    const int col_row_h = 17;
    const int left_x = CLOCK_PANEL_X + 16;
    const int right_x = CLOCK_PANEL_X + 154;
    const int col_w = 126;
    char uptime_buf[24];
    char ip_buf[24];
    char rssi_buf[24];
    char heap_buf[24];
    char heap_min_buf[28];
    char reset_buf[24];
    char running_buf[24];
    char next_buf[24];
    char weather_buf[24];
    char komari_buf[24];
    char version_buf[32];
    char ota_buf[32];
    weather_info_t weather = {0};
    komari_info_t komari = {0};
    ota_info_t ota = {0};
    esp_netif_ip_info_t ip_info = {0};
    wifi_ap_record_t ap_info = {0};
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    const esp_app_desc_t *app_desc = esp_app_get_description();
    size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t heap_min = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    size_t heap_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    format_uptime(uptime_buf, sizeof(uptime_buf));
    strlcpy(ip_buf, "--", sizeof(ip_buf));
    if (s_wifi_connected && s_sta_netif != NULL &&
        esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK &&
        ip_info.ip.addr != 0) {
        snprintf(ip_buf, sizeof(ip_buf), IPSTR, IP2STR(&ip_info.ip));
    }
    if (s_wifi_connected && esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        snprintf(rssi_buf, sizeof(rssi_buf), "%dDBM CH%d", ap_info.rssi, ap_info.primary);
    } else {
        strlcpy(rssi_buf, "--", sizeof(rssi_buf));
    }
    snprintf(heap_buf, sizeof(heap_buf), "%uK", (unsigned)(heap_free / 1024));
    snprintf(heap_min_buf, sizeof(heap_min_buf), "%uK/%uK",
             (unsigned)(heap_min / 1024), (unsigned)(heap_largest / 1024));
    strlcpy(reset_buf, reset_reason_text_zh(s_last_reset_reason), sizeof(reset_buf));
    format_partition_label(running, running_buf, sizeof(running_buf));
    format_partition_label(next, next_buf, sizeof(next_buf));

    weather_get_snapshot(&weather);
    format_time_age(weather.updated_at, weather_buf, sizeof(weather_buf));
    komari_get_snapshot(&komari);
    if (!komari.configured) {
        strlcpy(komari_buf, "关闭", sizeof(komari_buf));
    } else {
        format_time_age(komari.updated_at, komari_buf, sizeof(komari_buf));
    }
    strlcpy(version_buf, app_desc != NULL ? app_desc->version : "--", sizeof(version_buf));
    ota_get_snapshot(&ota);
    if (ota.status == OTA_STATUS_RUNNING && ota.progress_percent >= 0) {
        snprintf(ota_buf, sizeof(ota_buf), "%s %d%%", ota_status_text_zh(ota.status),
                 ota.progress_percent);
    } else if (ota.status == OTA_STATUS_FAILED && ota.last_error != ESP_OK) {
        snprintf(ota_buf, sizeof(ota_buf), "%s %s", ota_status_text_zh(ota.status),
                 esp_err_to_name(ota.last_error));
    } else {
        strlcpy(ota_buf, ota_status_text_zh(ota.status), sizeof(ota_buf));
    }

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(5, 9, 23), rgb565(9, 20, 33)),
                        TAG, "draw health bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("设备状态", tm, rgb565(94, 236, 218)), TAG, "draw health header failed");

    ESP_RETURN_ON_ERROR(draw_health_row(full_x, full_y0, full_w, "启动", uptime_buf, accent, text),
                        TAG, "draw uptime failed");
    ESP_RETURN_ON_ERROR(draw_health_row(full_x, full_y0 + full_row_h, full_w, "地址", ip_buf, muted, text),
                        TAG, "draw ip failed");

    ESP_RETURN_ON_ERROR(draw_health_row(left_x, col_y0 + col_row_h * 0, col_w, "信号", rssi_buf, muted, text),
                        TAG, "draw rssi failed");
    ESP_RETURN_ON_ERROR(draw_health_row(left_x, col_y0 + col_row_h * 1, col_w, "内存", heap_buf, muted, text),
                        TAG, "draw heap failed");
    ESP_RETURN_ON_ERROR(draw_health_row(left_x, col_y0 + col_row_h * 2, col_w, "低值", heap_min_buf, muted, text),
                        TAG, "draw heap min failed");
    ESP_RETURN_ON_ERROR(draw_health_row(left_x, col_y0 + col_row_h * 3, col_w, "重启", reset_buf, muted, text),
                        TAG, "draw reset failed");
    ESP_RETURN_ON_ERROR(draw_health_row(left_x, col_y0 + col_row_h * 4, col_w, "当前", running_buf, muted, text),
                        TAG, "draw run failed");

    ESP_RETURN_ON_ERROR(draw_health_row(right_x, col_y0 + col_row_h * 0, col_w, "下次", next_buf, muted, text),
                        TAG, "draw next failed");
    ESP_RETURN_ON_ERROR(draw_health_row(right_x, col_y0 + col_row_h * 1, col_w, "天气", weather_buf, muted, text),
                        TAG, "draw weather age failed");
    ESP_RETURN_ON_ERROR(draw_health_row(right_x, col_y0 + col_row_h * 2, col_w, "监控", komari_buf, muted, text),
                        TAG, "draw komari age failed");
    ESP_RETURN_ON_ERROR(draw_health_row(right_x, col_y0 + col_row_h * 3, col_w, "版本", version_buf, muted, text),
                        TAG, "draw version failed");
    ESP_RETURN_ON_ERROR(draw_health_row(right_x, col_y0 + col_row_h * 4, col_w, "更新", ota_buf, muted,
                                        ota.status == OTA_STATUS_FAILED ? warning : text),
                        TAG, "draw ota state failed");

    ESP_RETURN_ON_ERROR(draw_dialog_button(HEALTH_OTA_HIT_X, HEALTH_OTA_HIT_Y,
                                           HEALTH_OTA_HIT_W, HEALTH_OTA_HIT_H,
                                           "更新", true, button_bg),
                        TAG, "draw health ota button failed");
    return lcd_canvas_flush();
}

static void ota_format_source_label(const char *url, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    strlcpy(out, "来源 OTA", out_len);
    if (url == NULL || url[0] == '\0') {
        return;
    }

    if (strstr(url, "github.com/") != NULL) {
        strlcpy(out, "来源 GitHub", out_len);
        return;
    }

    const char *host = strstr(url, "://");
    host = host != NULL ? host + 3 : url;
    size_t host_len = strcspn(host, "/:?#");
    if (host_len == 0) {
        return;
    }

    char host_buf[40] = {0};
    if (host_len >= sizeof(host_buf)) {
        host_len = sizeof(host_buf) - 1;
    }
    memcpy(host_buf, host, host_len);
    host_buf[host_len] = '\0';
    snprintf(out, out_len, "来源 %s", host_buf);
}

static void ota_format_size_label(const ota_info_t *ota, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    strlcpy(out, "等待", out_len);
    if (ota == NULL || ota->image_read <= 0) {
        return;
    }

    int read_kb = (ota->image_read + 1023) / 1024;
    if (ota->image_size > 0) {
        int size_kb = (ota->image_size + 1023) / 1024;
        snprintf(out, out_len, "%dK/%dK", read_kb, size_kb);
    } else {
        snprintf(out, out_len, "%dK", read_kb);
    }
}

static esp_err_t draw_ota_progress_bar(int x, int y, int w, int h, int progress_percent,
                                       uint16_t border, uint16_t track, uint16_t fill)
{
    int progress = progress_percent;
    if (progress < 0) {
        progress = 0;
    } else if (progress > 100) {
        progress = 100;
    }

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, w, h, h / 2, border),
                        TAG, "draw ota progress border failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 2, y + 2, w - 4, h - 4,
                                            (h - 4) / 2, track),
                        TAG, "draw ota progress track failed");
    int fill_w = ((w - 4) * progress) / 100;
    if (fill_w > 0) {
        if (fill_w < h - 4) {
            fill_w = h - 4;
        }
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 2, y + 2, fill_w, h - 4,
                                                (h - 4) / 2, fill),
                            TAG, "draw ota progress fill failed");
    }
    return ESP_OK;
}

static esp_err_t draw_ota_confirm_screen(const struct tm *tm)
{
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(145, 161, 196);
    const uint16_t accent = rgb565(255, 211, 85);
    const uint16_t border = rgb565(84, 116, 160);
    const uint16_t progress_track = rgb565(24, 36, 58);
    const uint16_t progress_fill = rgb565(94, 236, 218);
    const uint16_t yes_bg = rgb565(255, 196, 76);
    ota_info_t ota = {0};
    char url[OTA_URL_LEN] = {0};
    char source_buf[48];
    char status_buf[48];
    char size_buf[24];
    char percent_buf[16];
    const char *title = "OTA更新";
    const char *left_button = "开始";
    bool show_buttons = true;

    ota_get_snapshot(&ota);
    (void)ota_get_effective_url(url, sizeof(url));
    ota_format_source_label(url, source_buf, sizeof(source_buf));
    ota_format_size_label(&ota, size_buf, sizeof(size_buf));
    strlcpy(status_buf, "准备更新", sizeof(status_buf));
    percent_buf[0] = '\0';

    if (ota.status == OTA_STATUS_RUNNING) {
        title = "正在下载";
        show_buttons = false;
        if (ota.progress_percent >= 0) {
            snprintf(status_buf, sizeof(status_buf), "下载中 %d%%", ota.progress_percent);
            snprintf(percent_buf, sizeof(percent_buf), "%d%%", ota.progress_percent);
        } else {
            strlcpy(status_buf, "连接中", sizeof(status_buf));
            strlcpy(percent_buf, "--", sizeof(percent_buf));
        }
    } else if (ota.status == OTA_STATUS_SUCCESS) {
        title = "更新完成";
        show_buttons = false;
        strlcpy(status_buf, "重启中", sizeof(status_buf));
        strlcpy(percent_buf, "100%", sizeof(percent_buf));
    } else if (ota.status == OTA_STATUS_FAILED) {
        title = "更新失败";
        left_button = "重试";
        if (ota.last_error != ESP_OK) {
            snprintf(status_buf, sizeof(status_buf), "失败 %s", esp_err_to_name(ota.last_error));
        } else {
            strlcpy(status_buf, "更新失败", sizeof(status_buf));
        }
    }

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(5, 8, 22), rgb565(12, 18, 34)),
                        TAG, "draw ota confirm bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("更新", tm, accent), TAG, "draw ota confirm header failed");

    const uint16_t box_accent = ota.status == OTA_STATUS_FAILED ? rgb565(255, 110, 120) :
                                (ota.status == OTA_STATUS_IDLE ? accent : progress_fill);
    const int inner_x = OTA_CONFIRM_BOX_X + 16;
    const int inner_w = OTA_CONFIRM_BOX_W - 32;
    ESP_RETURN_ON_ERROR(draw_dialog_box(OTA_CONFIRM_BOX_X, OTA_CONFIRM_BOX_Y,
                                        OTA_CONFIRM_BOX_W, OTA_CONFIRM_BOX_H, box_accent),
                        TAG, "draw ota confirm box failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_centered_limited(160, OTA_CONFIRM_BOX_Y + 10,
                                                         title, inner_w, 1, text),
                        TAG, "draw ota confirm title failed");
    ESP_RETURN_ON_ERROR(lcd_draw_hline(inner_x, OTA_CONFIRM_BOX_Y + 32, inner_w, rgb565(34, 46, 80)),
                        TAG, "draw ota divider failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(inner_x + 4, OTA_CONFIRM_BOX_Y + 46, 3, box_accent),
                        TAG, "draw ota status dot failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(inner_x + 12, OTA_CONFIRM_BOX_Y + 38,
                                                status_buf, inner_w - 12, 1,
                                                ota.status == OTA_STATUS_FAILED ? box_accent : text),
                        TAG, "draw ota status failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(inner_x + 12, OTA_CONFIRM_BOX_Y + 56,
                                                source_buf, inner_w - 12, 1, muted),
                        TAG, "draw ota source failed");

    if (ota.status == OTA_STATUS_RUNNING || ota.status == OTA_STATUS_SUCCESS) {
        ESP_RETURN_ON_ERROR(draw_ota_progress_bar(inner_x, OTA_CONFIRM_BOX_Y + 80,
                                                  inner_w, 12,
                                                  ota.status == OTA_STATUS_SUCCESS ? 100 :
                                                  ota.progress_percent,
                                                  border, progress_track, progress_fill),
                            TAG, "draw ota progress failed");
        if (percent_buf[0] != '\0') {
            ESP_RETURN_ON_ERROR(draw_text5(inner_x + inner_w - text5_width(percent_buf, 1),
                                           OTA_CONFIRM_BOX_Y + 100, percent_buf, 1,
                                           progress_fill),
                                TAG, "draw ota percent failed");
        }
        ESP_RETURN_ON_ERROR(draw_text_mixed_limited(inner_x, OTA_CONFIRM_BOX_Y + 96,
                                                    size_buf, inner_w - 40, 1, muted),
                            TAG, "draw ota size failed");
    } else {
        int body_w = text_mixed_width("下载重启", 1);
        ESP_RETURN_ON_ERROR(draw_text_mixed_limited(inner_x + inner_w - body_w, OTA_CONFIRM_BOX_Y + 38,
                                                    "下载重启", body_w, 1, muted),
                            TAG, "draw ota confirm body failed");
    }

    if (show_buttons) {
        ESP_RETURN_ON_ERROR(draw_dialog_button(OTA_CONFIRM_YES_X, OTA_CONFIRM_YES_Y,
                                               OTA_CONFIRM_BUTTON_W, OTA_CONFIRM_BUTTON_H,
                                               left_button, true, yes_bg),
                            TAG, "draw ota yes button failed");
        ESP_RETURN_ON_ERROR(draw_dialog_button(OTA_CONFIRM_NO_X, OTA_CONFIRM_NO_Y,
                                               OTA_CONFIRM_BUTTON_W, OTA_CONFIRM_BUTTON_H,
                                               "返回", false, accent),
                            TAG, "draw ota no button failed");
    }
    return lcd_canvas_flush();
}

// ---------------------------------------------------------------------------
// 更多页与小工具页面绘制。
// ---------------------------------------------------------------------------

static int clamp_int(int value, int min_value, int max_value);
static bool point_in_rect(int x, int y, int rect_x, int rect_y, int rect_w, int rect_h);

static void canvas_blend_pixel(int x, int y, uint16_t color, int alpha)
{
    if (alpha <= 0 || !lcd_canvas_is_active()) {
        return;
    }
    const int local_x = x - s_lcd.canvas_x;
    const int local_y = y - s_lcd.canvas_y;
    if (local_x < 0 || local_y < 0 || local_x >= s_lcd.canvas_w || local_y >= s_lcd.canvas_h) {
        return;
    }
    uint16_t *pixel = s_lcd.canvas_buf + (size_t)local_y * (size_t)s_lcd.canvas_w + (size_t)local_x;
    *pixel = alpha >= 255 ? color : mix_color(*pixel, color, alpha, 255);
}

static esp_err_t draw_aa_disc(float cx, float cy, float radius, uint16_t color)
{
    ESP_RETURN_ON_FALSE(lcd_canvas_is_active(), ESP_ERR_INVALID_STATE, TAG, "AA disc needs canvas");
    const int x0 = (int)floorf(cx - radius - 1.0f);
    const int x1 = (int)ceilf(cx + radius + 1.0f);
    const int y0 = (int)floorf(cy - radius - 1.0f);
    const int y1 = (int)ceilf(cy + radius + 1.0f);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float coverage = radius + 0.5f - sqrtf(dx * dx + dy * dy);
            if (coverage <= 0.0f) {
                continue;
            }
            canvas_blend_pixel(x, y, color, coverage >= 1.0f ? 255 : (int)(coverage * 255.0f));
        }
    }
    return ESP_OK;
}

static esp_err_t draw_progress_ring(int cx, int cy, int radius, int thickness, float fraction,
                                    uint16_t track, uint16_t color_start, uint16_t color_end)
{
    // 直接在画布像素上合成抗锯齿圆环：从 12 点方向顺时针绘制 fraction 比例的进度。
    ESP_RETURN_ON_FALSE(lcd_canvas_is_active(), ESP_ERR_INVALID_STATE, TAG, "progress ring needs canvas");
    if (fraction < 0.0f) {
        fraction = 0.0f;
    } else if (fraction > 1.0f) {
        fraction = 1.0f;
    }
    const float outer = (float)radius;
    const float inner = (float)(radius - thickness);
    const float sweep = fraction * 360.0f;
    const float rad_to_deg = 180.0f / (float)M_PI;

    for (int dy = -radius - 1; dy <= radius + 1; ++dy) {
        for (int dx = -radius - 1; dx <= radius + 1; ++dx) {
            float dist = sqrtf((float)(dx * dx + dy * dy));
            if (dist > outer + 0.5f || dist < inner - 0.5f) {
                continue;
            }
            float coverage = 1.0f;
            if (dist > outer - 0.5f) {
                coverage = outer + 0.5f - dist;
            }
            if (dist < inner + 0.5f) {
                float inner_cov = dist - (inner - 0.5f);
                if (inner_cov < coverage) {
                    coverage = inner_cov;
                }
            }
            float angle = atan2f((float)dy, (float)dx) * rad_to_deg + 90.0f;
            if (angle < 0.0f) {
                angle += 360.0f;
            }
            uint16_t color = track;
            if (fraction > 0.0f && angle <= sweep) {
                color = mix_color(color_start, color_end, (int)angle, 360);
            }
            canvas_blend_pixel(cx + dx, cy + dy, color, (int)(coverage * 255.0f));
        }
    }

    if (fraction <= 0.0f || fraction >= 1.0f) {
        return ESP_OK;
    }
    // 两端圆头，进度头部加一个亮点。
    const float mid = (float)radius - (float)thickness / 2.0f;
    const float cap = (float)thickness / 2.0f;
    const float head_rad = (sweep - 90.0f) / rad_to_deg;
    const float head_x = (float)cx + cosf(head_rad) * mid;
    const float head_y = (float)cy + sinf(head_rad) * mid;
    ESP_RETURN_ON_ERROR(draw_aa_disc((float)cx, (float)cy - mid, cap, color_start), TAG, "draw ring cap failed");
    ESP_RETURN_ON_ERROR(draw_aa_disc(head_x, head_y, cap, mix_color(color_start, color_end, (int)sweep, 360)),
                        TAG, "draw ring head failed");
    return draw_aa_disc(head_x, head_y, cap / 2.5f, rgb565(255, 255, 255));
}

static void more_tile_rect(int index, int *x, int *y)
{
    *x = MORE_TILE_X0 + (index % 2) * (MORE_TILE_W + MORE_TILE_GAP);
    *y = MORE_TILE_Y0 + (index / 2) * (MORE_TILE_H + MORE_TILE_GAP);
}

static app_view_t more_tile_target_view(int index)
{
    switch (index) {
    case 0:
        return APP_VIEW_POMODORO;
    case 1:
        return APP_VIEW_COUNTDOWN;
    case 2:
        return APP_VIEW_NIGHT;
    case 3:
        return APP_VIEW_ANALOG_CLOCK;
    default:
        return APP_VIEW_DASHBOARD;
    }
}

static esp_err_t draw_tomato_icon(int cx, int cy, uint16_t color)
{
    const uint16_t leaf = rgb565(96, 220, 120);
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy + 2, 9, color), TAG, "draw tomato failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx - 3, cy - 1, 2, mix_color(color, rgb565(255, 255, 255), 2, 5)),
                        TAG, "draw tomato shine failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(cx - 6, cy - 7, cx, cy - 4, cx - 1, cy - 9, leaf),
                        TAG, "draw tomato leaf failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(cx + 6, cy - 7, cx, cy - 4, cx + 1, cy - 9, leaf),
                        TAG, "draw tomato leaf failed");
    return lcd_fill_rect(cx, cy - 11, 2, 5, leaf);
}

static esp_err_t draw_hourglass_icon(int cx, int cy, uint16_t color)
{
    const uint16_t sand = rgb565(255, 211, 85);
    ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - 8, cy - 11, 17, 2, color), TAG, "draw hourglass top failed");
    ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - 8, cy + 10, 17, 2, color), TAG, "draw hourglass bottom failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(cx - 7, cy - 9, cx + 7, cy - 9, cx, cy, color),
                        TAG, "draw hourglass upper failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(cx - 7, cy + 9, cx + 7, cy + 9, cx, cy, color),
                        TAG, "draw hourglass lower failed");
    ESP_RETURN_ON_ERROR(lcd_fill_triangle(cx - 4, cy - 7, cx + 4, cy - 7, cx, cy - 2, sand),
                        TAG, "draw hourglass sand failed");
    return lcd_fill_triangle(cx - 5, cy + 9, cx + 5, cy + 9, cx, cy + 4, sand);
}

static esp_err_t draw_moon_icon(int cx, int cy, uint16_t color, uint16_t background)
{
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 9, color), TAG, "draw moon failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx + 5, cy - 4, 8, background), TAG, "draw moon cut failed");
    ESP_RETURN_ON_ERROR(lcd_fill_rect(cx + 7, cy + 4, 1, 1, color), TAG, "draw moon star failed");
    return lcd_fill_rect(cx + 9, cy - 9, 1, 1, color);
}

static esp_err_t draw_mini_clock_icon(int cx, int cy, uint16_t color, uint16_t background)
{
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 10, color), TAG, "draw mini clock failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, 8, background), TAG, "draw mini clock face failed");
    ESP_RETURN_ON_ERROR(lcd_draw_line(cx, cy, cx, cy - 6, 2, color), TAG, "draw mini clock hand failed");
    ESP_RETURN_ON_ERROR(lcd_draw_line(cx, cy, cx + 4, cy + 2, 2, color), TAG, "draw mini clock hand failed");
    return lcd_fill_circle(cx, cy, 1, rgb565(255, 255, 255));
}

static esp_err_t draw_more_tile(int index, const char *title, const char *subtitle,
                                uint16_t accent, bool active)
{
    int x = 0;
    int y = 0;
    more_tile_rect(index, &x, &y);
    const uint16_t border = mix_color(rgb565(26, 38, 74), accent, active ? 3 : 1, active ? 5 : 3);
    const uint16_t fill_top = mix_color(rgb565(16, 24, 52), accent, 1, active ? 5 : 8);
    const uint16_t fill_bottom = rgb565(8, 12, 30);
    const uint16_t icon_bg = mix_color(rgb565(10, 16, 38), accent, 1, 4);
    const uint16_t title_color = rgb565(232, 240, 255);
    const int icon_cx = x + 26;
    const int icon_cy = y + MORE_TILE_H / 2;

    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, MORE_TILE_W, MORE_TILE_H, 10, border),
                        TAG, "draw more tile border failed");
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 1, y + 1, MORE_TILE_W - 2, MORE_TILE_H - 2, 9, fill_bottom),
                        TAG, "draw more tile fill failed");
    for (int row = 0; row < MORE_TILE_H / 2; ++row) {
        int inset = row < 9 ? 9 - (int)sqrtf((float)(81 - (9 - row) * (9 - row))) : 0;
        ESP_RETURN_ON_ERROR(lcd_fill_rect(x + 1 + inset, y + 1 + row, MORE_TILE_W - 2 - inset * 2, 1,
                                          mix_color(fill_top, fill_bottom, row, MORE_TILE_H / 2)),
                            TAG, "draw more tile gradient failed");
    }
    ESP_RETURN_ON_ERROR(lcd_fill_circle(icon_cx, icon_cy, 17, icon_bg), TAG, "draw more tile icon bg failed");
    switch (index) {
    case 0:
        ESP_RETURN_ON_ERROR(draw_tomato_icon(icon_cx, icon_cy, accent), TAG, "draw tomato tile failed");
        break;
    case 1:
        ESP_RETURN_ON_ERROR(draw_hourglass_icon(icon_cx, icon_cy, accent), TAG, "draw hourglass tile failed");
        break;
    case 2:
        ESP_RETURN_ON_ERROR(draw_moon_icon(icon_cx - 1, icon_cy, accent, icon_bg), TAG, "draw moon tile failed");
        break;
    default:
        ESP_RETURN_ON_ERROR(draw_mini_clock_icon(icon_cx, icon_cy, accent, icon_bg), TAG, "draw clock tile failed");
        break;
    }

    const int text_x = x + 52;
    const int text_w = MORE_TILE_W - 58;
    ESP_RETURN_ON_ERROR(draw_text_mixed_limited(text_x, y + 13, title, text_w, 1, title_color),
                        TAG, "draw more tile title failed");
    int scale = text5_width(subtitle, 2) <= text_w ? 2 : 1;
    ESP_RETURN_ON_ERROR(draw_text5(text_x, y + MORE_TILE_H - (scale == 2 ? 24 : 20), subtitle, scale,
                                   active ? accent : mix_color(rgb565(140, 156, 190), accent, 1, 3)),
                        TAG, "draw more tile subtitle failed");
    if (active) {
        ESP_RETURN_ON_ERROR(lcd_fill_circle(x + MORE_TILE_W - 12, y + MORE_TILE_H - 14, 3, accent),
                            TAG, "draw more tile active dot failed");
    }
    return ESP_OK;
}

static void focus_timer_tile_subtitle(const focus_timer_t *timer, int idle_minutes, char *out, size_t out_len)
{
    if (timer->finished) {
        strlcpy(out, "DONE", out_len);
    } else if (timer->running || focus_timer_is_paused(timer)) {
        format_timer_mmss(focus_timer_remaining_seconds(timer), out, out_len);
    } else {
        snprintf(out, out_len, "%d MIN", idle_minutes);
    }
}

static esp_err_t draw_more_screen(const struct tm *tm)
{
    const uint16_t accent = rgb565(105, 207, 255);
    char pomodoro_buf[16];
    char countdown_buf[16];
    char night_buf[16];
    char clock_buf[8];

    focus_timer_tile_subtitle(&s_pomodoro_timer, s_pomodoro_presets[s_pomodoro_preset].minutes,
                              pomodoro_buf, sizeof(pomodoro_buf));
    focus_timer_tile_subtitle(&s_countdown_timer, s_countdown_minutes, countdown_buf, sizeof(countdown_buf));
    if (s_night.enabled) {
        snprintf(night_buf, sizeof(night_buf), "%02d-%02d", s_night.start_hour, s_night.end_hour);
    } else {
        strlcpy(night_buf, "OFF", sizeof(night_buf));
    }
    snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", tm->tm_hour, tm->tm_min);

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(6, 10, 28), rgb565(8, 14, 34)),
                        TAG, "draw more bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("更多", tm, accent), TAG, "draw more header failed");
    ESP_RETURN_ON_ERROR(draw_more_tile(0, "番茄钟", pomodoro_buf, rgb565(255, 99, 88),
                                       s_pomodoro_timer.running || s_pomodoro_timer.finished),
                        TAG, "draw pomodoro tile failed");
    ESP_RETURN_ON_ERROR(draw_more_tile(1, "倒计时", countdown_buf, rgb565(255, 185, 85),
                                       s_countdown_timer.running || s_countdown_timer.finished),
                        TAG, "draw countdown tile failed");
    ESP_RETURN_ON_ERROR(draw_more_tile(2, "夜间模式", night_buf, rgb565(176, 150, 255), s_night.enabled),
                        TAG, "draw night tile failed");
    ESP_RETURN_ON_ERROR(draw_more_tile(3, "时钟", clock_buf, rgb565(76, 176, 255), false),
                        TAG, "draw clock tile failed");
    return lcd_canvas_flush();
}

static void timer_side_button_rect(int col, int y, int *x, int *w)
{
    *x = TIMER_SIDE_X + col * (TIMER_SIDE_BUTTON_W + TIMER_SIDE_GAP);
    *w = TIMER_SIDE_BUTTON_W;
    (void)y;
}

static void pomodoro_preset_rect(int index, int *x, int *y, int *w, int *h)
{
    timer_side_button_rect(index % 2, 0, x, w);
    *y = TIMER_PRESET_Y0 + (index / 2) * (POMODORO_PRESET_H + TIMER_SIDE_GAP);
    *h = POMODORO_PRESET_H;
}

static void countdown_preset_rect(int index, int *x, int *y, int *w, int *h)
{
    timer_side_button_rect(index % 2, 0, x, w);
    *y = TIMER_PRESET_Y0 + (index / 2) * (COUNTDOWN_PRESET_H + COUNTDOWN_PRESET_GAP);
    *h = COUNTDOWN_PRESET_H;
}

static const char *focus_timer_primary_label(const focus_timer_t *timer)
{
    if (timer->running) {
        return "暂停";
    }
    if (focus_timer_is_paused(timer)) {
        return "继续";
    }
    return "开始";
}

static esp_err_t draw_timer_controls(const focus_timer_t *timer, uint16_t accent)
{
    int x = 0;
    int w = 0;
    timer_side_button_rect(0, TIMER_CONTROL_Y, &x, &w);
    ESP_RETURN_ON_ERROR(draw_dialog_button(x, TIMER_CONTROL_Y, w, TIMER_CONTROL_H,
                                           focus_timer_primary_label(timer), true, accent),
                        TAG, "draw timer primary button failed");
    timer_side_button_rect(1, TIMER_CONTROL_Y, &x, &w);
    return draw_dialog_button(x, TIMER_CONTROL_Y, w, TIMER_CONTROL_H, "重置", false, accent);
}

static esp_err_t draw_timer_dial(const focus_timer_t *timer, const char *phase_label,
                                 uint16_t color_start, uint16_t color_end, int blink_second)
{
    const int cx = TIMER_RING_CX;
    const int cy = TIMER_RING_CY;
    const uint16_t text = rgb565(240, 245, 255);
    const uint16_t muted = rgb565(142, 158, 191);
    const uint16_t done = rgb565(76, 235, 142);
    const uint16_t track = mix_color(rgb565(18, 26, 55), color_start, 1, 6);
    char time_buf[8];
    float fraction = focus_timer_fraction(timer);
    uint16_t ring_start = color_start;
    uint16_t ring_end = color_end;

    if (timer->finished) {
        // 结束后整圈闪烁提醒，直到用户操作。
        fraction = 1.0f;
        ring_start = (blink_second % 2) == 0 ? done : mix_color(track, done, 1, 3);
        ring_end = ring_start;
    }

    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, TIMER_RING_RADIUS + 4, rgb565(10, 16, 38)),
                        TAG, "draw timer halo failed");
    ESP_RETURN_ON_ERROR(draw_progress_ring(cx, cy, TIMER_RING_RADIUS, TIMER_RING_THICKNESS, fraction,
                                           track, ring_start, ring_end),
                        TAG, "draw timer ring failed");
    ESP_RETURN_ON_ERROR(lcd_fill_circle(cx, cy, TIMER_RING_RADIUS - TIMER_RING_THICKNESS - 4,
                                        mix_color(rgb565(8, 12, 30), color_start, 1, 12)),
                        TAG, "draw timer face failed");

    format_timer_mmss(focus_timer_remaining_seconds(timer), time_buf, sizeof(time_buf));
    ESP_RETURN_ON_ERROR(draw_text_mixed_centered_limited(cx, cy - 38, phase_label, 90, 1,
                                                         mix_color(muted, color_start, 1, 2)),
                        TAG, "draw timer phase failed");
    ESP_RETURN_ON_ERROR(draw_text5_centered(cx, cy - 11, time_buf, 3, timer->finished ? done : text),
                        TAG, "draw timer digits failed");

    const char *state = NULL;
    uint16_t state_color = muted;
    if (timer->finished) {
        state = "完成";
        state_color = done;
    } else if (timer->running) {
        state = NULL;
    } else if (focus_timer_is_paused(timer)) {
        state = "暂停";
        state_color = rgb565(255, 211, 85);
    } else {
        state = "准备";
    }
    if (state != NULL) {
        ESP_RETURN_ON_ERROR(draw_text_mixed_centered(cx, cy + 16, state, 1, state_color),
                            TAG, "draw timer state failed");
    }
    return ESP_OK;
}

static esp_err_t draw_pomodoro_screen(const struct tm *tm)
{
    const uint16_t accent = rgb565(255, 99, 88);
    const pomodoro_preset_t *current = &s_pomodoro_presets[s_pomodoro_preset];
    const bool focus = current->phase == POMODORO_PHASE_FOCUS;
    const uint16_t ring_start = focus ? rgb565(255, 120, 96) : rgb565(30, 226, 214);
    const uint16_t ring_end = focus ? rgb565(255, 196, 76) : rgb565(76, 235, 142);

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(14, 8, 24), rgb565(8, 12, 32)),
                        TAG, "draw pomodoro bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("番茄钟", tm, accent), TAG, "draw pomodoro header failed");
    ESP_RETURN_ON_ERROR(draw_timer_dial(&s_pomodoro_timer, current->label, ring_start, ring_end, tm->tm_sec),
                        TAG, "draw pomodoro dial failed");

    // 已完成的番茄数：每 4 个一组，用小圆点显示当前组进度。
    int filled = s_pomodoro_completed == 0 ? 0 :
                 ((s_pomodoro_completed - 1) % POMODORO_ROUNDS_PER_LONG_BREAK) + 1;
    for (int i = 0; i < POMODORO_ROUNDS_PER_LONG_BREAK; ++i) {
        int dot_x = TIMER_RING_CX - 21 + i * 14;
        int dot_y = TIMER_RING_CY + 42;
        uint16_t dot = i < filled ? accent : rgb565(44, 50, 84);
        ESP_RETURN_ON_ERROR(lcd_fill_circle(dot_x, dot_y, 4, dot), TAG, "draw pomodoro dot failed");
    }

    for (int i = 0; i < POMODORO_PRESET_COUNT; ++i) {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        char minutes_buf[8];
        const pomodoro_preset_t *preset = &s_pomodoro_presets[i];
        const bool selected = i == s_pomodoro_preset;
        const uint16_t preset_accent = preset->phase == POMODORO_PHASE_FOCUS ? accent : rgb565(30, 226, 214);
        const uint16_t fill = selected ? mix_color(rgb565(16, 22, 44), preset_accent, 1, 3) : rgb565(16, 22, 44);
        const uint16_t border = selected ? preset_accent : mix_color(rgb565(30, 40, 70), preset_accent, 1, 3);

        pomodoro_preset_rect(i, &x, &y, &w, &h);
        snprintf(minutes_buf, sizeof(minutes_buf), "%d", preset->minutes);
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, w, h, 9, border), TAG, "draw pomodoro preset failed");
        ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x + 1, y + 1, w - 2, h - 2, 8, fill),
                            TAG, "draw pomodoro preset fill failed");
        ESP_RETURN_ON_ERROR(draw_text_mixed_centered(x + w / 2, y + 6, preset->label, 1,
                                                     selected ? rgb565(255, 255, 255) : rgb565(196, 210, 238)),
                            TAG, "draw pomodoro preset label failed");
        ESP_RETURN_ON_ERROR(draw_text5_centered(x + w / 2, y + h - 19, minutes_buf, 2, preset_accent),
                            TAG, "draw pomodoro preset minutes failed");
    }
    ESP_RETURN_ON_ERROR(draw_timer_controls(&s_pomodoro_timer, accent), TAG, "draw pomodoro controls failed");
    return lcd_canvas_flush();
}

static esp_err_t draw_countdown_screen(const struct tm *tm)
{
    const uint16_t accent = rgb565(255, 185, 85);
    char label_buf[24];

    snprintf(label_buf, sizeof(label_buf), "%d 分", s_countdown_minutes);
    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(14, 10, 22), rgb565(8, 12, 30)),
                        TAG, "draw countdown bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("倒计时", tm, accent), TAG, "draw countdown header failed");
    ESP_RETURN_ON_ERROR(draw_timer_dial(&s_countdown_timer, label_buf, rgb565(255, 211, 85),
                                        rgb565(255, 120, 170), tm->tm_sec),
                        TAG, "draw countdown dial failed");

    for (int i = 0; i < COUNTDOWN_PRESET_COUNT; ++i) {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        char preset_buf[16];
        countdown_preset_rect(i, &x, &y, &w, &h);
        snprintf(preset_buf, sizeof(preset_buf), "%d分", s_countdown_presets[i]);
        ESP_RETURN_ON_ERROR(draw_dialog_button(x, y, w, h, preset_buf,
                                               s_countdown_presets[i] == s_countdown_minutes, accent),
                            TAG, "draw countdown preset failed");
    }
    ESP_RETURN_ON_ERROR(draw_timer_controls(&s_countdown_timer, accent), TAG, "draw countdown controls failed");
    return lcd_canvas_flush();
}

static int night_row_y(int row)
{
    return NIGHT_ROW_Y0 + row * (NIGHT_ROW_H + NIGHT_ROW_GAP);
}

static esp_err_t draw_night_toggle(int x, int y, bool on, uint16_t accent)
{
    const uint16_t track = on ? accent : rgb565(44, 52, 86);
    const int knob_r = NIGHT_TOGGLE_H / 2 - 3;
    const int knob_x = on ? x + NIGHT_TOGGLE_W - NIGHT_TOGGLE_H / 2 : x + NIGHT_TOGGLE_H / 2;
    ESP_RETURN_ON_ERROR(lcd_fill_round_rect(x, y, NIGHT_TOGGLE_W, NIGHT_TOGGLE_H, NIGHT_TOGGLE_H / 2, track),
                        TAG, "draw toggle track failed");
    ESP_RETURN_ON_ERROR(draw_text5(on ? x + 9 : x + NIGHT_TOGGLE_W - 26, y + 10, on ? "ON" : "OFF", 1,
                                   on ? rgb565(20, 16, 40) : rgb565(150, 160, 190)),
                        TAG, "draw toggle text failed");
    return lcd_fill_circle(knob_x, y + NIGHT_TOGGLE_H / 2, knob_r, rgb565(240, 245, 255));
}

static esp_err_t draw_step_button(int x, int y, int w, int h, bool plus, uint16_t accent)
{
    const int cx = x + w / 2;
    const int cy = y + h / 2;
    ESP_RETURN_ON_ERROR(draw_dialog_button(x, y, w, h, "", false, accent), TAG, "draw step button failed");
    ESP_RETURN_ON_ERROR(lcd_fill_rect(cx - 6, cy - 1, 13, 3, accent), TAG, "draw step minus failed");
    if (!plus) {
        return ESP_OK;
    }
    return lcd_fill_rect(cx - 1, cy - 6, 3, 13, accent);
}

static esp_err_t draw_night_screen(const struct tm *tm)
{
    const uint16_t accent = rgb565(176, 150, 255);
    const uint16_t text = rgb565(232, 240, 255);
    const uint16_t muted = rgb565(142, 158, 191);
    const uint16_t line = rgb565(32, 42, 76);
    const bool active = night_mode_active(tm);
    static const char *const labels[NIGHT_ROW_COUNT] = {"开始", "结束", "亮度"};
    char values[NIGHT_ROW_COUNT][8];

    snprintf(values[0], sizeof(values[0]), "%02d:00", s_night.start_hour);
    snprintf(values[1], sizeof(values[1]), "%02d:00", s_night.end_hour);
    snprintf(values[2], sizeof(values[2]), "%d%%", s_night.dim_percent);

    ESP_RETURN_ON_ERROR(draw_view_background(rgb565(8, 6, 24), rgb565(6, 10, 26)),
                        TAG, "draw night bg failed");
    ESP_RETURN_ON_ERROR(draw_view_header("夜间模式", tm, accent), TAG, "draw night header failed");

    ESP_RETURN_ON_ERROR(draw_moon_icon(NIGHT_ROW_X + 9, NIGHT_TOGGLE_Y + NIGHT_TOGGLE_H / 2,
                                       s_night.enabled ? accent : muted, rgb565(8, 8, 26)),
                        TAG, "draw night moon failed");
    ESP_RETURN_ON_ERROR(draw_text_mixed(NIGHT_ROW_X + 26, NIGHT_TOGGLE_Y + 5, "自动调暗", 1, text),
                        TAG, "draw night title failed");
    if (active) {
        ESP_RETURN_ON_ERROR(draw_text_mixed(NIGHT_ROW_X + 26 + text_mixed_width("自动调暗", 1) + 8,
                                            NIGHT_TOGGLE_Y + 5, "生效中", 1, rgb565(255, 211, 85)),
                            TAG, "draw night active failed");
    }
    ESP_RETURN_ON_ERROR(draw_night_toggle(NIGHT_ROW_RIGHT - NIGHT_TOGGLE_W, NIGHT_TOGGLE_Y, s_night.enabled, accent),
                        TAG, "draw night toggle failed");
    ESP_RETURN_ON_ERROR(lcd_draw_hline(NIGHT_ROW_X, NIGHT_ROW_Y0 - 5, NIGHT_ROW_RIGHT - NIGHT_ROW_X, line),
                        TAG, "draw night divider failed");

    for (int row = 0; row < NIGHT_ROW_COUNT; ++row) {
        const int y = night_row_y(row);
        const uint16_t row_accent = s_night.enabled ? accent : mix_color(muted, accent, 1, 3);
        const char *value = values[row];
        if (row == 2 && s_night.dim_percent == 0) {
            value = NULL;
        }
        ESP_RETURN_ON_ERROR(draw_text_mixed(NIGHT_ROW_X, y + (NIGHT_ROW_H - 16) / 2, labels[row], 1, muted),
                            TAG, "draw night row label failed");
        if (value != NULL) {
            ESP_RETURN_ON_ERROR(draw_text5_centered(NIGHT_VALUE_CX, y + (NIGHT_ROW_H - 14) / 2, value, 2,
                                                    s_night.enabled ? text : muted),
                                TAG, "draw night row value failed");
        } else {
            ESP_RETURN_ON_ERROR(draw_text_mixed_centered(NIGHT_VALUE_CX, y + (NIGHT_ROW_H - 16) / 2, "关屏", 1,
                                                         s_night.enabled ? text : muted),
                                TAG, "draw night row value failed");
        }
        ESP_RETURN_ON_ERROR(draw_step_button(NIGHT_MINUS_X, y, NIGHT_STEP_BUTTON_W, NIGHT_ROW_H, false, row_accent),
                            TAG, "draw night minus failed");
        ESP_RETURN_ON_ERROR(draw_step_button(NIGHT_PLUS_X, y, NIGHT_STEP_BUTTON_W, NIGHT_ROW_H, true, row_accent),
                            TAG, "draw night plus failed");
    }

    char hint_buf[48];
    snprintf(hint_buf, sizeof(hint_buf), "夜间轻触屏幕亮起 %d 秒", NIGHT_TOUCH_WAKE_MS / 1000);
    ESP_RETURN_ON_ERROR(draw_text_mixed_centered(160, night_row_y(NIGHT_ROW_COUNT) + 4, hint_buf, 1, muted),
                        TAG, "draw night hint failed");
    return lcd_canvas_flush();
}

static bool point_in_timer_ring(int x, int y)
{
    int dx = x - TIMER_RING_CX;
    int dy = y - TIMER_RING_CY;
    return dx * dx + dy * dy <= (TIMER_RING_RADIUS + 6) * (TIMER_RING_RADIUS + 6);
}

static bool timer_controls_handle_touch(focus_timer_t *timer, int x, int y)
{
    int bx = 0;
    int bw = 0;
    timer_side_button_rect(0, TIMER_CONTROL_Y, &bx, &bw);
    if (point_in_rect(x, y, bx, TIMER_CONTROL_Y, bw, TIMER_CONTROL_H) || point_in_timer_ring(x, y)) {
        focus_timer_toggle(timer);
        return true;
    }
    timer_side_button_rect(1, TIMER_CONTROL_Y, &bx, &bw);
    if (point_in_rect(x, y, bx, TIMER_CONTROL_Y, bw, TIMER_CONTROL_H)) {
        focus_timer_reset(timer);
        return true;
    }
    return false;
}

static bool pomodoro_handle_touch(int x, int y)
{
    for (int i = 0; i < POMODORO_PRESET_COUNT; ++i) {
        int px = 0;
        int py = 0;
        int pw = 0;
        int ph = 0;
        pomodoro_preset_rect(i, &px, &py, &pw, &ph);
        if (point_in_rect(x, y, px, py, pw, ph)) {
            pomodoro_apply_preset(i, true);
            return true;
        }
    }
    return timer_controls_handle_touch(&s_pomodoro_timer, x, y);
}

static bool countdown_handle_touch(int x, int y)
{
    for (int i = 0; i < COUNTDOWN_PRESET_COUNT; ++i) {
        int px = 0;
        int py = 0;
        int pw = 0;
        int ph = 0;
        countdown_preset_rect(i, &px, &py, &pw, &ph);
        if (point_in_rect(x, y, px, py, pw, ph)) {
            countdown_apply_minutes(s_countdown_presets[i], true);
            return true;
        }
    }
    return timer_controls_handle_touch(&s_countdown_timer, x, y);
}

static bool night_handle_touch(int x, int y)
{
    if (point_in_rect(x, y, NIGHT_ROW_X, NIGHT_TOGGLE_Y - 4, NIGHT_ROW_RIGHT - NIGHT_ROW_X, NIGHT_TOGGLE_H + 8)) {
        s_night.enabled = !s_night.enabled;
        night_config_save();
        return true;
    }
    for (int row = 0; row < NIGHT_ROW_COUNT; ++row) {
        const int y0 = night_row_y(row);
        int delta = 0;
        if (point_in_rect(x, y, NIGHT_MINUS_X, y0, NIGHT_STEP_BUTTON_W, NIGHT_ROW_H)) {
            delta = -1;
        } else if (point_in_rect(x, y, NIGHT_PLUS_X, y0, NIGHT_STEP_BUTTON_W, NIGHT_ROW_H)) {
            delta = 1;
        } else {
            continue;
        }
        if (row == 0) {
            s_night.start_hour = (uint8_t)((s_night.start_hour + 24 + delta) % 24);
        } else if (row == 1) {
            s_night.end_hour = (uint8_t)((s_night.end_hour + 24 + delta) % 24);
        } else {
            int level = night_dim_level_index() + delta;
            level = clamp_int(level, 0, NIGHT_DIM_LEVEL_COUNT - 1);
            s_night.dim_percent = s_night_dim_levels[level];
        }
        night_config_save();
        return true;
    }
    return false;
}

static esp_err_t draw_current_view(const struct tm *tm, app_view_t view)
{
    switch (view) {
    case APP_VIEW_TODAY_WEATHER:
        return draw_today_weather_screen(tm);
    case APP_VIEW_WIFI_CONFIRM:
        return draw_wifi_reprovision_confirm_screen(tm);
    case APP_VIEW_OTA_CONFIRM:
        return draw_ota_confirm_screen(tm);
    case APP_VIEW_ANALOG_CLOCK:
        return draw_analog_clock_screen(tm);
    case APP_VIEW_CALENDAR:
        return draw_calendar_screen(tm);
    case APP_VIEW_WEATHER:
        return draw_weather_screen(tm);
    case APP_VIEW_KOMARI:
        return draw_komari_screen(tm);
    case APP_VIEW_HEALTH:
        return draw_health_screen(tm);
    case APP_VIEW_MORE:
        return draw_more_screen(tm);
    case APP_VIEW_POMODORO:
        return draw_pomodoro_screen(tm);
    case APP_VIEW_COUNTDOWN:
        return draw_countdown_screen(tm);
    case APP_VIEW_NIGHT:
        return draw_night_screen(tm);
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
        "<label>Komari 地址（可选）</label><input name='komari_url' maxlength='191' autocomplete='off' "
        "placeholder='https://komari.example.com'>"
        "<p class='hint'>填写后首页会显示 Komari 在线状态栏，留空则关闭。</p>"
        "<label>OTA 地址（可选）</label><input name='ota_url' maxlength='255' autocomplete='off' "
        "placeholder='https://github.com/lengyuesky/esp32-2432s028r_clock/releases/download/ota-latest/cyd_clock.bin'>"
        "<p class='hint'>留空使用默认 GitHub Releases HTTPS OTA 地址，仅支持 HTTPS。</p>"
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
    char komari_url[KOMARI_URL_LEN] = {0};
    char normalized_komari_url[KOMARI_URL_LEN] = {0};
    char ota_url[OTA_URL_LEN] = {0};
    char normalized_ota_url[OTA_URL_LEN] = {0};
    form_get_value(form, "ssid", ssid, sizeof(ssid));
    form_get_value(form, "password", password, sizeof(password));
    form_get_value(form, "komari_url", komari_url, sizeof(komari_url));
    form_get_value(form, "ota_url", ota_url, sizeof(ota_url));
    char *trimmed_ssid = trim_ascii(ssid);
    char *trimmed_komari_url = trim_ascii(komari_url);
    char *trimmed_ota_url = trim_ascii(ota_url);
    if (strlen(trimmed_ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty ssid");
        return ESP_FAIL;
    }
    if (!komari_normalize_url(trimmed_komari_url, normalized_komari_url,
                              sizeof(normalized_komari_url))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid komari url");
        return ESP_FAIL;
    }
    if (!ota_normalize_url(trimmed_ota_url, normalized_ota_url, sizeof(normalized_ota_url))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid ota url");
        return ESP_FAIL;
    }

    esp_err_t ret = wifi_credentials_save(trimmed_ssid, password);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
        return ESP_FAIL;
    }
    ret = komari_config_save(normalized_komari_url);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save komari failed");
        return ESP_FAIL;
    }
    ret = ota_config_save(normalized_ota_url);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save ota failed");
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

static esp_err_t provisioning_start(bool saved_credentials_available)
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
    s_wifi_configured = saved_credentials_available;
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

    ESP_LOGI(TAG, "provisioning portal started: SSID=%s URL=%s auth=%s saved_creds=%d",
             s_prov_ap_ssid, s_captive_portal_uri,
             ap_config.ap.authmode == WIFI_AUTH_OPEN ? "open" : "wpa2",
             saved_credentials_available);
    return ESP_OK;
#else
    (void)saved_credentials_available;
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

    BaseType_t task_ok = xTaskCreate(weather_task, "weather", WEATHER_TASK_STACK_SIZE, NULL, 4,
                                     &s_weather_task);
    if (task_ok == pdPASS) {
        s_weather_task_started = true;
    } else {
        s_weather_task = NULL;
        ESP_LOGE(TAG, "create weather task failed");
    }
#endif
}

static void start_komari_task_once(void)
{
    if (s_komari_task_started || s_wifi_event_group == NULL || !komari_configured()) {
        return;
    }

    BaseType_t task_ok = xTaskCreate(komari_task, "komari", KOMARI_TASK_STACK_SIZE, NULL, 4,
                                     &s_komari_task);
    if (task_ok == pdPASS) {
        s_komari_task_started = true;
    } else {
        s_komari_task = NULL;
        ESP_LOGE(TAG, "create Komari task failed");
    }
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
    start_komari_task_once();

    for (;;) {
        wifi_credentials_t creds = {0};
        bool saved_credentials_available = false;
        bool force_provisioning = false;
        ret = wifi_credentials_load(&creds);
        if (ret == ESP_OK) {
            saved_credentials_available = true;
        }

        force_provisioning = wifi_reprovision_request_take();
        if (force_provisioning) {
            ESP_LOGI(TAG, "manual provisioning requested");
        }

        if (ret == ESP_OK && !force_provisioning) {
            ESP_LOGI(TAG, "try Wi-Fi SSID=%s source=%s", creds.ssid, creds.from_nvs ? "nvs" : "kconfig");
            provisioning_stop();
            ret = wifi_connect_with_credentials(&creds, pdMS_TO_TICKS(20000));
            if (ret == ESP_OK) {
                start_komari_task_once();
                bool new_credentials = false;
                bool retry_saved_credentials = false;
                for (;;) {
                    if (!s_time_synced) {
                        (void)sync_time_with_sntp();
                    }
                    TickType_t wait_ticks = s_time_synced ? portMAX_DELAY : pdMS_TO_TICKS(60000);
                    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                                           WIFI_FAIL_BIT | WIFI_PROV_DONE_BIT |
                                                           WIFI_FORCE_PROV_BIT,
                                                           pdTRUE, pdFALSE, wait_ticks);
                    if (bits & WIFI_PROV_DONE_BIT) {
                        new_credentials = true;
                        break;
                    }
                    if (bits & WIFI_FORCE_PROV_BIT) {
                        s_wifi_force_prov_pending = false;
                        force_provisioning = true;
                        ESP_LOGI(TAG, "manual provisioning requested");
                        break;
                    }
                    if (bits & WIFI_FAIL_BIT) {
                        ESP_LOGW(TAG, "Wi-Fi lost after retries, keep saved credentials and open temporary provisioning");
                        retry_saved_credentials = true;
                        break;
                    }
                }
                if (new_credentials) {
                    continue;
                }
                if (retry_saved_credentials) {
                    ESP_LOGI(TAG, "saved Wi-Fi credentials remain available for automatic retry");
                }
            }
        } else if (ret != ESP_OK) {
            ESP_LOGI(TAG, "no Wi-Fi credentials found");
        }

        ret = provisioning_start(saved_credentials_available);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "provisioning unavailable: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        TickType_t provisioning_wait_ticks = saved_credentials_available ?
                                             pdMS_TO_TICKS(WIFI_SAVED_CREDENTIALS_RETRY_MS) :
                                             portMAX_DELAY;
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_PROV_DONE_BIT,
                                               pdTRUE, pdFALSE, provisioning_wait_ticks);
        if (bits & WIFI_PROV_DONE_BIT) {
            ESP_LOGI(TAG, "new Wi-Fi credentials submitted");
        } else if (saved_credentials_available) {
            ESP_LOGI(TAG, "no new Wi-Fi credentials submitted, retry saved Wi-Fi credentials");
            provisioning_stop();
        }
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
    case APP_VIEW_TODAY_WEATHER:
        return "today-weather";
    case APP_VIEW_WIFI_CONFIRM:
        return "wifi-confirm";
    case APP_VIEW_OTA_CONFIRM:
        return "ota-confirm";
    case APP_VIEW_ANALOG_CLOCK:
        return "time";
    case APP_VIEW_CALENDAR:
        return "calendar";
    case APP_VIEW_WEATHER:
        return "weather";
    case APP_VIEW_KOMARI:
        return "komari";
    case APP_VIEW_HEALTH:
        return "health";
    case APP_VIEW_MORE:
        return "more";
    case APP_VIEW_POMODORO:
        return "pomodoro";
    case APP_VIEW_COUNTDOWN:
        return "countdown";
    case APP_VIEW_NIGHT:
        return "night";
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

    if (backlight_handle_touch_wake()) {
        ESP_LOGI(TAG, "touch woke dimmed backlight at (%d,%d)", x, y);
        return false;
    }

    app_view_t next_view = *view;
    int nav_x = 0;
    int nav_w = 0;
    bool show_komari = komari_configured();
    int nav_count = nav_card_count(show_komari);
    bool redraw = false;

    if (*view == APP_VIEW_OTA_CONFIRM) {
        ota_info_t ota = {0};
        ota_get_snapshot(&ota);
        if (ota.status == OTA_STATUS_RUNNING || ota.status == OTA_STATUS_SUCCESS) {
            redraw = true;
        } else if (point_in_rect(x, y, OTA_CONFIRM_YES_X, OTA_CONFIRM_YES_Y,
                                 OTA_CONFIRM_BUTTON_W, OTA_CONFIRM_BUTTON_H)) {
            (void)ota_start_update();
            redraw = true;
        } else if (point_in_rect(x, y, OTA_CONFIRM_NO_X, OTA_CONFIRM_NO_Y,
                                 OTA_CONFIRM_BUTTON_W, OTA_CONFIRM_BUTTON_H) ||
                   !point_in_rect(x, y, OTA_CONFIRM_BOX_X, OTA_CONFIRM_BOX_Y,
                                  OTA_CONFIRM_BOX_W, OTA_CONFIRM_BOX_H)) {
            next_view = APP_VIEW_HEALTH;
        }
    } else if (*view == APP_VIEW_WIFI_CONFIRM) {
        if (point_in_rect(x, y, WIFI_CONFIRM_YES_X, WIFI_CONFIRM_YES_Y,
                          WIFI_CONFIRM_BUTTON_W, WIFI_CONFIRM_BUTTON_H)) {
            request_wifi_reprovision();
            next_view = APP_VIEW_DASHBOARD;
        } else if (point_in_rect(x, y, WIFI_CONFIRM_NO_X, WIFI_CONFIRM_NO_Y,
                                 WIFI_CONFIRM_BUTTON_W, WIFI_CONFIRM_BUTTON_H) ||
                   !point_in_rect(x, y, WIFI_CONFIRM_BOX_X, WIFI_CONFIRM_BOX_Y,
                                  WIFI_CONFIRM_BOX_W, WIFI_CONFIRM_BOX_H)) {
            next_view = APP_VIEW_DASHBOARD;
        }
    } else if (point_in_rect(x, y, WIFI_STATUS_HIT_X, WIFI_STATUS_HIT_Y,
                             WIFI_STATUS_HIT_W, WIFI_STATUS_HIT_H)) {
        next_view = APP_VIEW_WIFI_CONFIRM;
    } else if (*view == APP_VIEW_HEALTH) {
        if (point_in_rect(x, y, HEALTH_OTA_HIT_X, HEALTH_OTA_HIT_Y,
                          HEALTH_OTA_HIT_W, HEALTH_OTA_HIT_H)) {
            next_view = APP_VIEW_OTA_CONFIRM;
        } else {
            next_view = APP_VIEW_DASHBOARD;
        }
    } else if (*view == APP_VIEW_TODAY_WEATHER) {
        if (x >= CLOCK_PANEL_X + CLOCK_PANEL_W / 2) {
            if (s_today_hourly_page < TODAY_HOURLY_PAGE_COUNT - 1) {
                ++s_today_hourly_page;
                redraw = true;
            }
        } else if (s_today_hourly_page > 0) {
            --s_today_hourly_page;
            redraw = true;
        } else {
            next_view = APP_VIEW_DASHBOARD;
        }
    } else if (*view == APP_VIEW_MORE) {
        next_view = APP_VIEW_DASHBOARD;
        for (int i = 0; i < MORE_TILE_COUNT; ++i) {
            int tile_x = 0;
            int tile_y = 0;
            more_tile_rect(i, &tile_x, &tile_y);
            if (point_in_rect(x, y, tile_x, tile_y, MORE_TILE_W, MORE_TILE_H)) {
                next_view = more_tile_target_view(i);
                break;
            }
        }
    } else if (*view == APP_VIEW_POMODORO) {
        redraw = pomodoro_handle_touch(x, y);
        if (!redraw) {
            next_view = APP_VIEW_MORE;
        }
    } else if (*view == APP_VIEW_COUNTDOWN) {
        redraw = countdown_handle_touch(x, y);
        if (!redraw) {
            next_view = APP_VIEW_MORE;
        }
    } else if (*view == APP_VIEW_NIGHT) {
        redraw = night_handle_touch(x, y);
        if (!redraw) {
            next_view = APP_VIEW_MORE;
        }
    } else if (*view != APP_VIEW_DASHBOARD) {
        next_view = APP_VIEW_DASHBOARD;
    } else if (point_in_rect(x, y, DASHBOARD_WEATHER_HIT_X, DASHBOARD_WEATHER_HIT_Y,
                             DASHBOARD_WEATHER_HIT_W, DASHBOARD_WEATHER_HIT_H)) {
        s_today_hourly_page = 0;
        next_view = APP_VIEW_TODAY_WEATHER;
    } else {
        for (int i = 0; i < nav_count; ++i) {
            nav_card_rect(i, nav_count, &nav_x, &nav_w);
            if (point_in_rect(x, y, nav_x, NAV_CARD_Y, nav_w, NAV_CARD_H)) {
                next_view = nav_card_target_view(i, show_komari);
                break;
            }
        }
    }
    if (next_view == *view && *view == APP_VIEW_DASHBOARD &&
        point_in_rect(x, y, HEALTH_STATUS_HIT_X, HEALTH_STATUS_HIT_Y,
                      HEALTH_STATUS_HIT_W, HEALTH_STATUS_HIT_H)) {
        next_view = APP_VIEW_HEALTH;
    } else if (next_view == *view && *view == APP_VIEW_DASHBOARD &&
        point_in_rect(x, y, CLOCK_PANEL_X + 28, CLOCK_PANEL_Y + 48,
                      CLOCK_PANEL_W - 56, 60)) {
        next_view = APP_VIEW_ANALOG_CLOCK;
    } else if (next_view == *view && *view == APP_VIEW_DASHBOARD &&
               point_in_rect(x, y, CLOCK_PANEL_X + 14, CLOCK_PANEL_Y + 112, 196, 42)) {
        next_view = APP_VIEW_CALENDAR;
    }

    ESP_LOGI(TAG, "touch raw=(%u,%u) pressure=%u irq=%d screen=(%d,%d) view=%s",
             raw_x, raw_y, pressure, touch_irq_level(), x, y, view_name(next_view));
    if (next_view != *view) {
        *view = next_view;
        return true;
    }
    return redraw;
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
    esp_err_t bl_ret = backlight_init();
    if (bl_ret != ESP_OK) {
        // PWM 不可用时退回普通 GPIO 开关，夜间模式只能整屏开/关。
        ESP_LOGW(TAG, "backlight PWM unavailable, fallback to GPIO: %s", esp_err_to_name(bl_ret));
        s_backlight_percent = 100;
    }
    ESP_LOGI(TAG, "LCD initialized: ILI9341 SPI 320x240 RGB565");
    return ESP_OK;
}

void app_main(void)
{
    setenv("TZ", CONFIG_CLOCK_TIMEZONE, 1);
    tzset();
    esp_reset_reason_t reset_reason = esp_reset_reason();
    s_last_reset_reason = reset_reason;
    ESP_LOGI(TAG, "boot reset_reason=%s(%d)", reset_reason_name(reset_reason), (int)reset_reason);
    if (reset_reason == ESP_RST_BROWNOUT) {
        ESP_LOGW(TAG, "last reset was brownout; check USB cable, power supply and backlight current");
    }
    ota_mark_running_app_valid();
    log_runtime_health("boot");
    set_time_from_build();
    s_boot_time = time(NULL);

    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(network_fetch_lock_init());
    ESP_ERROR_CHECK(weather_state_init());
    ESP_ERROR_CHECK(komari_state_init());
    ESP_ERROR_CHECK(komari_config_load());
    ESP_ERROR_CHECK(ota_state_init());
    ESP_ERROR_CHECK(ota_config_load());
    if (night_config_load() != ESP_OK) {
        ESP_LOGW(TAG, "load night mode config failed, using defaults");
    }
    focus_tools_init();
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(touch_init());
    ESP_ERROR_CHECK(draw_static_background());
    ESP_ERROR_CHECK(draw_text_mixed_centered(160, 112, "启动中", 1, rgb565(114, 238, 255)));

    BaseType_t network_ok = xTaskCreate(network_task, "network", 8192, NULL, 5, NULL);
    if (network_ok != pdPASS) {
        ESP_LOGE(TAG, "create network task failed");
    }
    BaseType_t serial_ok = xTaskCreate(serial_command_task, "serial_cmd",
                                       SERIAL_COMMAND_TASK_STACK_SIZE, NULL, 2, NULL);
    if (serial_ok != pdPASS) {
        ESP_LOGE(TAG, "create serial command task failed");
    }

    int last_second = -1;
    int last_timer_tick = -1;
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

        // 计时结束：自动跳到对应页面并点亮背光提醒（OTA 确认/升级中不打断）。
        if (focus_timer_poll_finished(&s_pomodoro_timer)) {
            pomodoro_on_finished();
            backlight_wake(TIMER_ALERT_WAKE_MS);
            if (s_current_view != APP_VIEW_OTA_CONFIRM) {
                s_current_view = APP_VIEW_POMODORO;
            }
            force_redraw = true;
        }
        if (focus_timer_poll_finished(&s_countdown_timer)) {
            ESP_LOGI(TAG, "countdown finished (%d min)", s_countdown_minutes);
            backlight_wake(TIMER_ALERT_WAKE_MS);
            if (s_current_view != APP_VIEW_OTA_CONFIRM) {
                s_current_view = APP_VIEW_COUNTDOWN;
            }
            force_redraw = true;
        }
        backlight_update(&tm);

        if (s_current_view != last_view) {
            last_view = s_current_view;
            force_redraw = true;
        }

        // 计时页面按计时器自己的秒跳动刷新，避免与系统时钟秒边界错位造成跳秒。
        const focus_timer_t *view_timer = s_current_view == APP_VIEW_POMODORO ? &s_pomodoro_timer :
                                          (s_current_view == APP_VIEW_COUNTDOWN ? &s_countdown_timer : NULL);
        bool tick_changed = tm.tm_sec != last_second;
        if (view_timer != NULL && view_timer->running) {
            int timer_tick = focus_timer_remaining_seconds(view_timer);
            tick_changed = timer_tick != last_timer_tick || (tick_changed && tm.tm_sec == 0);
            last_timer_tick = timer_tick;
        } else {
            last_timer_tick = -1;
        }

        if (force_redraw || tick_changed) {
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
