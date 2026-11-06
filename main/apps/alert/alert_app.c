#include "alert_app.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "esp_codec_dev.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ALERT_TONE_FREQ_HZ 880.0f
#define ALERT_PULSE_ON_MS 80U
#define ALERT_PULSE_OFF_MS 80U

typedef struct {
    bool is_on;
    bool audio_stop_requested;
    bool audio_running;
    lv_obj_t *screen;
    lv_obj_t *toggle_button;
    lv_obj_t *icon_label;
    lv_obj_t *text_label;
    TaskHandle_t audio_task;
} alert_app_state_t;

static const char *TAG = "alert_app";

static void alert_apply_button_state(alert_app_state_t *state)
{
    if (state == NULL || state->toggle_button == NULL || state->icon_label == NULL || state->text_label == NULL) {
        return;
    }

    if (state->is_on) {
        lv_obj_set_style_bg_color(state->toggle_button, lv_color_hex(0x1a8f5c), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(state->toggle_button, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(state->toggle_button, lv_color_hex(0x9df8c2), LV_PART_MAIN);
        lv_obj_set_style_border_width(state->toggle_button, 2, LV_PART_MAIN);
        lv_label_set_text(state->icon_label, "🔔");
        lv_label_set_text(state->text_label, "Alert On");
    } else {
        lv_obj_set_style_bg_color(state->toggle_button, lv_color_hex(0x1f232a), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(state->toggle_button, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(state->toggle_button, lv_color_hex(0xb6bdc7), LV_PART_MAIN);
        lv_obj_set_style_border_width(state->toggle_button, 1, LV_PART_MAIN);
        lv_label_set_text(state->icon_label, "🔕");
        lv_label_set_text(state->text_label, "Alert Off");
    }
}

static void alert_audio_stop(alert_app_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->audio_stop_requested = true;
    state->audio_running = false;
}

static void alert_audio_task(void *arg)
{
    alert_app_state_t *state = (alert_app_state_t *)arg;
    if (state == NULL) {
        vTaskDelete(NULL);
        return;
    }

    esp_err_t init_result = bsp_extra_codec_init();
    if (init_result != ESP_OK) {
        ESP_LOGW(TAG, "Audio codec init failed: %s", esp_err_to_name(init_result));
        state->audio_running = false;
        vTaskDelete(NULL);
        return;
    }

    if (!bsp_extra_audio_acquire(BSP_EXTRA_AUDIO_OWNER_MUSIC, 250)) {
        ESP_LOGW(TAG, "Audio session unavailable; alert output blocked");
        state->audio_running = false;
        vTaskDelete(NULL);
        return;
    }

    esp_err_t fs_result = bsp_extra_codec_set_fs(8000, 16, I2S_SLOT_MODE_MONO);
    if (fs_result != ESP_OK) {
        ESP_LOGW(TAG, "Failed to configure board audio for alert output: %s", esp_err_to_name(fs_result));
        bsp_extra_audio_release();
        state->audio_running = false;
        vTaskDelete(NULL);
        return;
    }

    bsp_extra_codec_volume_set(100, NULL);

    int16_t tone[128];
    int16_t silence[128] = {0};
    for (size_t i = 0; i < 128; ++i) {
        const float phase = (2.0f * (float)M_PI * ALERT_TONE_FREQ_HZ * (float)i) / 8000.0f;
        tone[i] = (int16_t)(0.85f * sinf(phase) * 32767.0f);
    }

    state->audio_running = true;
    state->audio_stop_requested = false;
    ESP_LOGI(TAG, "Starting alert tone at max volume");

    uint32_t pulse_elapsed_ms = 0U;
    const uint32_t pulse_period_ms = ALERT_PULSE_ON_MS + ALERT_PULSE_OFF_MS;

    while (!state->audio_stop_requested && state->is_on) {
        const bool pulse_on = (pulse_elapsed_ms % pulse_period_ms) < ALERT_PULSE_ON_MS;
        int16_t *chunk = pulse_on ? tone : silence;
        size_t written = 0;
        esp_err_t write_result = bsp_extra_i2s_write(chunk, sizeof(tone), &written, 100);
        if (write_result != ESP_OK) {
            ESP_LOGW(TAG, "Alert write failed: %s", esp_err_to_name(write_result));
            break;
        }
        pulse_elapsed_ms += 20U;
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    bsp_extra_audio_release();
    state->audio_running = false;
    state->audio_stop_requested = false;
    ESP_LOGI(TAG, "Alert tone stopped");
    vTaskDelete(NULL);
}

static void alert_start_audio(alert_app_state_t *state)
{
    if (state == NULL || state->audio_running || !state->is_on) {
        return;
    }

    state->audio_stop_requested = false;
    BaseType_t status = xTaskCreatePinnedToCore(
        alert_audio_task,
        "alert_audio",
        6144,
        state,
        5,
        &state->audio_task,
        1
    );

    if (status != pdPASS) {
        ESP_LOGE(TAG, "Failed to create alert audio task");
        state->audio_running = false;
        return;
    }
}

static void alert_toggle_cb(lv_event_t *event)
{
    alert_app_state_t *state = (alert_app_state_t *)lv_event_get_user_data(event);
    if (state == NULL) {
        return;
    }

    state->is_on = !state->is_on;
    ESP_LOGI(TAG, "Alert state changed: %s", state->is_on ? "ON" : "OFF");

    if (state->is_on) {
        alert_start_audio(state);
    } else {
        alert_audio_stop(state);
    }

    alert_apply_button_state(state);
}

static void alert_delete_cb(lv_event_t *event)
{
    alert_app_state_t *state = (alert_app_state_t *)lv_event_get_user_data(event);
    if (state == NULL) {
        return;
    }

    state->is_on = false;
    alert_audio_stop(state);
    state->screen = NULL;
}

void bike_alert_app_open(const bike_app_t *app, lv_obj_t *screen)
{
    (void)app;
    if (screen == NULL) {
        return;
    }

    bike_app_layout_t layout = {0};
    bike_app_layout_init(screen, "Alert", &layout);

    alert_app_state_t *state = calloc(1, sizeof(*state));
    if (state == NULL) {
        ESP_LOGE(TAG, "Failed to allocate alert state");
        return;
    }

    state->screen = screen;
    state->is_on = false;

    state->toggle_button = lv_btn_create(layout.content);
    lv_obj_set_size(state->toggle_button, BIKE_LAYOUT_CARD_WIDTH, BIKE_LAYOUT_CARD_HEIGHT);
    lv_obj_align(state->toggle_button, LV_ALIGN_CENTER, 0, BIKE_LAYOUT_CARD_CENTER_Y);
    lv_obj_set_style_bg_color(state->toggle_button, lv_color_hex(0x1f232a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(state->toggle_button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(state->toggle_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(state->toggle_button, lv_color_hex(0xb6bdc7), LV_PART_MAIN);

    state->icon_label = lv_label_create(state->toggle_button);
    lv_obj_align(state->icon_label, LV_ALIGN_CENTER, 0, BIKE_LAYOUT_CARD_ICON_Y);
    lv_obj_set_style_text_font(state->icon_label, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_color(state->icon_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(state->icon_label, "🔕");

    state->text_label = lv_label_create(state->toggle_button);
    lv_obj_align(state->text_label, LV_ALIGN_CENTER, 0, BIKE_LAYOUT_CARD_TEXT_Y);
    lv_obj_set_style_text_font(state->text_label, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_color(state->text_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(state->text_label, "Alert Off");

    lv_obj_add_event_cb(state->toggle_button, alert_toggle_cb, LV_EVENT_CLICKED, state);
    alert_apply_button_state(state);
    lv_obj_add_event_cb(screen, alert_delete_cb, LV_EVENT_DELETE, state);
}
