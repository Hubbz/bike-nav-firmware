#include "security_app.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "bsp/esp-bsp.h"
#include "bsp/board.h"
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

#define ARM_ALARM_POLL_MS 100U // Polling interval for motion detection in ms
#define ARM_ALARM_ARMED_DELAY_MS 2000U // Continuous movement before sounding
#define ARM_ALARM_IDLE_STOP_MS 15000U // Idle time before stopping alarm
#define ARM_ALARM_ALERT_FREQ_HZ 880.0f // Alert tone frequency in Hz
#define ARM_ALARM_PULSE_ON_MS 80U // Pulse on duration in ms
#define ARM_ALARM_PULSE_OFF_MS 80U // Pulse off duration in ms
#define ARM_ALARM_START_MOTION_G 0.16f // Motion threshold to start alarm in g
#define ARM_ALARM_CONTINUE_MOTION_G 0.10f // Motion threshold to continue alarm in g

typedef struct {
    bool is_armed;
    bool imu_ready;
    bool alarm_active;
    bool audio_stop_requested;
    bool audio_running;
    uint32_t motion_ms;
    uint32_t idle_ms;
    lv_obj_t *screen;
    lv_obj_t *toggle_button;
    lv_obj_t *icon_label;
    lv_obj_t *text_label;
    lv_timer_t *motion_timer;
    TaskHandle_t audio_task;
} arm_alarm_app_state_t;

static const char *TAG = "security_app";

static void arm_alarm_apply_button_state(arm_alarm_app_state_t *state)
{
    if (state == NULL || state->toggle_button == NULL || state->icon_label == NULL || state->text_label == NULL) {
        return;
    }

    if (state->is_armed) {
        lv_obj_set_style_bg_color(state->toggle_button, lv_color_hex(0x1a8f5c), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(state->toggle_button, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(state->toggle_button, lv_color_hex(0x9df8c2), LV_PART_MAIN);
        lv_obj_set_style_border_width(state->toggle_button, 2, LV_PART_MAIN);
        lv_label_set_text(state->icon_label, "🔒");
        lv_label_set_text(state->text_label, "Armed");
    } else {
        lv_obj_set_style_bg_color(state->toggle_button, lv_color_hex(0x1f232a), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(state->toggle_button, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(state->toggle_button, lv_color_hex(0xb6bdc7), LV_PART_MAIN);
        lv_obj_set_style_border_width(state->toggle_button, 1, LV_PART_MAIN);
        lv_label_set_text(state->icon_label, "🔓");
        lv_label_set_text(state->text_label, "Disarmed");
    }
}

static void arm_alarm_audio_stop(arm_alarm_app_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->audio_stop_requested = true;
    state->audio_running = false;
    state->alarm_active = false;
}

static void arm_alarm_audio_task(void *arg)
{
    arm_alarm_app_state_t *state = (arm_alarm_app_state_t *)arg;
    if (state == NULL) {
        vTaskDelete(NULL);
        return;
    }

    esp_err_t init_result = bsp_extra_codec_init();
    if (init_result != ESP_OK) {
        ESP_LOGW(TAG, "Audio codec init failed: %s", esp_err_to_name(init_result));
        state->audio_running = false;
        state->alarm_active = false;
        vTaskDelete(NULL);
        return;
    }

    if (!bsp_extra_audio_acquire(BSP_EXTRA_AUDIO_OWNER_MUSIC, 250)) {
        ESP_LOGW(TAG, "Audio session unavailable; alarm output blocked");
        state->motion_ms = 0U;
        state->audio_running = false;
        state->alarm_active = false;
        vTaskDelete(NULL);
        return;
    }

    esp_err_t fs_result = bsp_extra_codec_set_fs(8000, 16, I2S_SLOT_MODE_MONO);
    if (fs_result != ESP_OK) {
        ESP_LOGW(TAG, "Failed to configure board audio for alarm output: %s", esp_err_to_name(fs_result));
        bsp_extra_audio_release();
        state->motion_ms = 0U;
        state->audio_running = false;
        state->alarm_active = false;
        vTaskDelete(NULL);
        return;
    }

    bsp_extra_codec_volume_set(100, NULL);

    int16_t tone[128];
    int16_t silence[128] = {0};
    for (size_t i = 0; i < 128; ++i) {
        const float phase = (2.0f * (float)M_PI * ARM_ALARM_ALERT_FREQ_HZ * (float)i) / 8000.0f;
        tone[i] = (int16_t)(0.85f * sinf(phase) * 32767.0f);
    }

    state->audio_running = true;
    state->audio_stop_requested = false;
    ESP_LOGI(TAG, "Starting alarm tone at max volume");

    uint32_t pulse_elapsed_ms = 0U;
    const uint32_t pulse_period_ms = ARM_ALARM_PULSE_ON_MS + ARM_ALARM_PULSE_OFF_MS;

    while (!state->audio_stop_requested) {
        const bool pulse_on = (pulse_elapsed_ms % pulse_period_ms) < ARM_ALARM_PULSE_ON_MS;
        int16_t *chunk = pulse_on ? tone : silence;
        size_t written = 0;
        esp_err_t write_result = bsp_extra_i2s_write(chunk, sizeof(tone), &written, 100);
        if (write_result != ESP_OK) {
            ESP_LOGW(TAG, "Alarm write failed: %s", esp_err_to_name(write_result));
            break;
        }
        pulse_elapsed_ms += 20U;
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    bsp_extra_audio_release();
    state->audio_running = false;
    state->audio_stop_requested = false;
    state->alarm_active = false;
    ESP_LOGI(TAG, "Alarm tone stopped");
    vTaskDelete(NULL);
}

static bool arm_alarm_imu_init(arm_alarm_app_state_t *state)
{
    if (state == NULL) {
        return false;
    }

    if (state->imu_ready) {
        return true;
    }

    esp_err_t ret = bsp_imu_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "IMU init failed: %s", esp_err_to_name(ret));
        return false;
    }

    state->imu_ready = true;
    ESP_LOGI(TAG, "QMI8658 armed motion monitor ready");
    return true;
}

static float arm_alarm_accel_g_mag(const bsp_imu_data_t *sample)
{
    return sqrtf((sample->accel_x * sample->accel_x) +
                 (sample->accel_y * sample->accel_y) +
                 (sample->accel_z * sample->accel_z));
}

static void arm_alarm_start_audio(arm_alarm_app_state_t *state)
{
    if (state == NULL || state->audio_running || !state->is_armed) {
        return;
    }

    ESP_LOGI(TAG, "arm_alarm_start_audio() called");
    state->audio_stop_requested = false;
    state->alarm_active = true;
    BaseType_t status = xTaskCreatePinnedToCore(
        arm_alarm_audio_task,
        "arm_alarm_audio",
        6144,
        state,
        5,
        &state->audio_task,
        1
    );

    if (status != pdPASS) {
        ESP_LOGE(TAG, "Failed to create alarm audio task");
        state->audio_running = false;
        state->alarm_active = false;
        return;
    }

    ESP_LOGI(TAG, "Audio task created");
}

static void arm_alarm_motion_update_cb(lv_timer_t *timer)
{
    arm_alarm_app_state_t *state = (arm_alarm_app_state_t *)lv_timer_get_user_data(timer);
    if (state == NULL || !state->is_armed || state->screen == NULL) {
        return;
    }

    if (!arm_alarm_imu_init(state)) {
        return;
    }

    bsp_imu_data_t sample = {0};
    esp_err_t ret = bsp_imu_get_data(&sample);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "IMU read failed: %s", esp_err_to_name(ret));
        return;
    }

    const float raw_mag = arm_alarm_accel_g_mag(&sample);
    const float motion_signal = fabsf(raw_mag - 1.0f);
    const float motion_threshold = state->alarm_active ? ARM_ALARM_CONTINUE_MOTION_G : ARM_ALARM_START_MOTION_G;
    const bool motion_detected = motion_signal > motion_threshold;

    if (motion_detected) {
        state->motion_ms += ARM_ALARM_POLL_MS;
        state->idle_ms = 0U;
        if (state->motion_ms >= ARM_ALARM_ARMED_DELAY_MS && !state->alarm_active) {
            ESP_LOGI(TAG, "Motion held for %u ms, starting alarm", ARM_ALARM_ARMED_DELAY_MS);
            arm_alarm_start_audio(state);
        }
    } else {
        state->idle_ms += ARM_ALARM_POLL_MS;
        if (state->motion_ms > 0U) {
            if (state->motion_ms > ARM_ALARM_POLL_MS) {
                state->motion_ms -= ARM_ALARM_POLL_MS;
            } else {
                state->motion_ms = 0U;
            }
        }
        if (state->alarm_active && state->idle_ms >= ARM_ALARM_IDLE_STOP_MS) {
            arm_alarm_audio_stop(state);
        }
    }

}

static void arm_alarm_toggle_cb(lv_event_t *event)
{
    arm_alarm_app_state_t *state = (arm_alarm_app_state_t *)lv_event_get_user_data(event);
    if (state == NULL) {
        return;
    }

    state->is_armed = !state->is_armed;
    state->motion_ms = 0U;
    state->idle_ms = 0U;
    ESP_LOGI(TAG, "Security state changed: %s", state->is_armed ? "ARMED" : "DISARMED");
    if (!state->is_armed) {
        arm_alarm_audio_stop(state);
    }
    arm_alarm_apply_button_state(state);
}

static void arm_alarm_delete_cb(lv_event_t *event)
{
    arm_alarm_app_state_t *state = (arm_alarm_app_state_t *)lv_event_get_user_data(event);
    if (state == NULL) {
        return;
    }

    if (state->motion_timer != NULL) {
        lv_timer_del(state->motion_timer);
        state->motion_timer = NULL;
    }

    state->is_armed = false;
    arm_alarm_audio_stop(state);
    state->screen = NULL;
}

void bike_arm_alarm_app_open(const bike_app_t *app, lv_obj_t *screen)
{
    (void)app;
    if (screen == NULL) {
        return;
    }

    bike_app_layout_t layout = {0};
    bike_app_layout_init(screen, "Security", &layout);

    arm_alarm_app_state_t *state = calloc(1, sizeof(*state));
    if (state == NULL) {
        ESP_LOGE(TAG, "Failed to allocate arm alarm state");
        return;
    }

    state->screen = screen;
    state->is_armed = false;
    state->alarm_active = false;
    state->motion_ms = 0U;
    state->idle_ms = 0U;

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
    lv_label_set_text(state->icon_label, "🔓");

    state->text_label = lv_label_create(state->toggle_button);
    lv_obj_align(state->text_label, LV_ALIGN_CENTER, 0, BIKE_LAYOUT_CARD_TEXT_Y);
    lv_obj_set_style_text_font(state->text_label, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_color(state->text_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(state->text_label, "Disarmed");

    lv_obj_add_event_cb(state->toggle_button, arm_alarm_toggle_cb, LV_EVENT_CLICKED, state);
    arm_alarm_apply_button_state(state);

    state->motion_timer = lv_timer_create(arm_alarm_motion_update_cb, ARM_ALARM_POLL_MS, state);
    lv_obj_add_event_cb(screen, arm_alarm_delete_cb, LV_EVENT_DELETE, state);
}
