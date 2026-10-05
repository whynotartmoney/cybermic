// Cyber Mick City. Display, buttons, and battery go through the BSP.
// The catalog and canvas live in main. All on-screen copy is English.
// Catalog: UP/DOWN move COUNT, COLOR, TEMPO, PAPER; OK cycles the focused value;
// long OK starts the canvas. The catalog opens on gray.
// The canvas shows the form name at the top left and the battery percent at the top right.
// UP recolors, DOWN changes form, long OK returns. Count runs 1 through 5.
// Color is MONO or MULTI. Paper is WHITE, GRAY, BLACK, BEIGE, CREAM, or WHEAT.
// Tempo is 1s, 2s, 5s, 10s, DRIFT, or IMPACT.
// Catalog idle for 45 seconds dims the backlight; the canvas stays bright.
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "mick_field.h"
#include "mick_ui.h"

#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "mick";

#define INPUT_QUEUE_DEPTH 8
#define FRAME_MS 40
#define IDLE_DIM_US (45LL * 1000 * 1000)
#define BATTERY_PERIOD_US (5LL * 1000 * 1000)
#define BACKLIGHT_ACTIVE 72
#define BACKLIGHT_IDLE 14

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static volatile bool s_input_ready;
static mick_field_t s_field;

static bool map_input(bsp_btn_t btn, bsp_btn_ev_t event, mick_input_t *input)
{
    if (event == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        *input = MICK_INPUT_OK_LONG;
        return true;
    }
    if (event != BSP_BTN_CLICK) return false;
    if (btn == BSP_BTN_UP) *input = MICK_INPUT_UP;
    else if (btn == BSP_BTN_DOWN) *input = MICK_INPUT_DOWN;
    else if (btn == BSP_BTN_OK) *input = MICK_INPUT_OK;
    else return false;
    return true;
}

static void app_task(void *arg)
{
    (void)arg;
    int64_t last = esp_timer_get_time();
    int64_t last_input = last;
    int64_t last_battery = 0;
    int battery = -1;
    uint8_t backlight = 0xFF;

    for (;;) {
        input_event_t event;
        bool got = xQueueReceive(s_input_queue, &event, pdMS_TO_TICKS(FRAME_MS)) == pdTRUE;
        int64_t now = esp_timer_get_time();
        int64_t delta = now - last;
        last = now;
        if (delta < 0) delta = 0;
        if (delta > 250000) delta = 250000;
        uint32_t dt_ms = (uint32_t)(delta / 1000);

        if (got) {
            last_input = now;
            mick_input_t input;
            if (map_input(event.btn, event.event, &input)) {
                mick_field_on_input(&s_field, input);
            }
        }
        mick_field_tick(&s_field, dt_ms);

        if (last_battery == 0 || now - last_battery >= BATTERY_PERIOD_US) {
            battery = bsp_battery_soc();
            last_battery = now;
        }

        uint8_t level = BACKLIGHT_ACTIVE;
        if (s_field.phase != MICK_PHASE_CANVAS && now - last_input >= IDLE_DIM_US) {
            level = BACKLIGHT_IDLE;
        }
        if (level != backlight) {
            bsp_display_backlight(level);
            backlight = level;
        }

        if (bsp_lvgl_lock(200)) {
            mick_ui_sync(&s_field, battery);
            s_field.dirty = 0;
            bsp_lvgl_unlock();
        }
    }
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || s_input_queue == NULL) return;
    const input_event_t input = {.btn = btn, .event = event};
    (void)xQueueSend(s_input_queue, &input, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "cyber mick city");
    mick_field_init(&s_field, 0x4D49434Bu);

    bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || bsp_lvgl_init() == NULL) {
        ESP_LOGE(TAG, "display init failed MOSI=%d SCLK=%d CS=%d DC=%d BL=%d",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(BACKLIGHT_ACTIVE);

    if (bsp_battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "battery gauge unavailable");
    }

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (s_input_queue == NULL) {
        ESP_LOGE(TAG, "input queue create failed");
        return;
    }

    int battery = bsp_battery_soc();
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "lvgl lock failed");
        return;
    }
    mick_ui_create(&s_field, battery);
    s_field.dirty = 0;
    bsp_lvgl_unlock();

    if (xTaskCreate(app_task, "mick_app", 6144, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "ui task create failed");
        return;
    }
    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "button init failed");
        return;
    }
    s_input_ready = true;
}
