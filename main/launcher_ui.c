#include "launcher_ui.h"

#include <stdio.h>
#include <stdlib.h>

#include "app_registry.h"
#include "esp_log.h"
#include "system_status.h"
#include "ui_config.h"

static const char *TAG = "launcher_ui";

#if LVGL_AVAILABLE
static const int16_t SWIPE_THRESHOLD_PX = 35;
static const int16_t TAP_THRESHOLD_PX = 12;

static const bike_app_t *s_apps = NULL;
static size_t s_app_count = 0;
static size_t s_current_app_index = 0;

static lv_obj_t *s_home_screen = NULL;
static lv_obj_t *s_splash_screen = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_battery_label = NULL;
static lv_obj_t *s_app_card = NULL;
static lv_obj_t *s_app_icon_label = NULL;
static lv_obj_t *s_app_name_label = NULL;
static lv_obj_t *s_app_hint_label = NULL;
static lv_obj_t *s_no_apps_label = NULL;
static lv_obj_t *s_app_screen = NULL;
static lv_obj_t *s_splash_label = NULL;

static int16_t s_press_start_x = 0;

static void set_screen_black(lv_obj_t *screen)
{
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
}

static void set_text_white(lv_obj_t *obj)
{
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
}

static void set_text_opa_anim(void *obj, int32_t value)
{
    lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)value, LV_PART_MAIN);
}

static void add_black_backdrop(lv_obj_t *screen)
{
    lv_obj_t *backdrop = lv_obj_create(screen);
    lv_obj_set_size(backdrop, LV_PCT(100), LV_PCT(100));
    lv_obj_center(backdrop);
    lv_obj_set_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(backdrop, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(backdrop, 0, LV_PART_MAIN);
}

static void update_status_bar(void)
{
    if (s_time_label == NULL || s_battery_label == NULL) {
        return;
    }

    char time_text[6];
    bike_status_get_time_text(time_text, sizeof(time_text));
    lv_label_set_text(s_time_label, time_text);

    char battery_text[8];
    const unsigned int battery_percent = (unsigned int)bike_status_get_battery_percent();
    snprintf(battery_text, sizeof(battery_text), "%u%%", battery_percent);
    lv_label_set_text(s_battery_label, battery_text);
}

static void status_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_status_bar();
}

static void refresh_app_card(void)
{
    if (s_app_card == NULL) {
        return;
    }

    if (s_app_count == 0U) {
        if (s_no_apps_label != NULL) {
            lv_obj_set_hidden(s_no_apps_label, false);
        }
        lv_label_set_text(s_app_hint_label, "");
        lv_obj_set_hidden(s_app_card, true);
        return;
    }

    if (s_no_apps_label != NULL) {
        lv_obj_set_hidden(s_no_apps_label, true);
    }
    lv_obj_set_hidden(s_app_card, false);

    const bike_app_t *app = &s_apps[s_current_app_index];
    lv_label_set_text(s_app_icon_label, app->icon);
    lv_label_set_text(s_app_name_label, app->name);

    char hint_text[64];
    snprintf(
        hint_text,
        sizeof(hint_text),
        "Swipe left/right (%u/%u) | Tap to open",
        (unsigned int)(s_current_app_index + 1U),
        (unsigned int)s_app_count
    );
    lv_label_set_text(s_app_hint_label, hint_text);
}

static void show_home_screen(void)
{
    if (s_home_screen == NULL) {
        return;
    }

    lv_scr_load(s_home_screen);
    refresh_app_card();
    update_status_bar();
}

static void app_screen_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_RELEASED) {
        return;
    }
    show_home_screen();
}

static void open_current_app(void)
{
    if (s_app_count == 0U) {
        return;
    }

    if (!bike_app_open(s_current_app_index)) {
        ESP_LOGW(TAG, "Failed to open app at index %u", (unsigned int)s_current_app_index);
        return;
    }

    if (s_app_screen != NULL) {
        lv_obj_del(s_app_screen);
        s_app_screen = NULL;
    }

    const bike_app_t *app = &s_apps[s_current_app_index];
    s_app_screen = lv_obj_create(NULL);
    lv_obj_set_flag(s_app_screen, LV_OBJ_FLAG_SCROLLABLE, false);
    set_screen_black(s_app_screen);

    lv_obj_t *icon = lv_label_create(s_app_screen);
    lv_label_set_text(icon, app->icon);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -24);
    set_text_white(icon);

    lv_obj_t *name = lv_label_create(s_app_screen);
    lv_label_set_text(name, app->name);
    lv_obj_align_to(name, icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    set_text_white(name);

    lv_obj_t *hint = lv_label_create(s_app_screen);
    lv_label_set_text(hint, "Tap anywhere to return home");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);
    set_text_white(hint);

    lv_obj_add_event_cb(s_app_screen, app_screen_event_cb, LV_EVENT_RELEASED, NULL);
    lv_scr_load(s_app_screen);
}

static void app_card_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        s_press_start_x = point.x;
        return;
    }

    if (code != LV_EVENT_RELEASED || s_app_count == 0U) {
        return;
    }

    const int16_t delta_x = point.x - s_press_start_x;

    if (delta_x >= SWIPE_THRESHOLD_PX && s_current_app_index > 0U) {
        s_current_app_index--;
        refresh_app_card();
        return;
    }

    if (delta_x <= -SWIPE_THRESHOLD_PX && (s_current_app_index + 1U) < s_app_count) {
        s_current_app_index++;
        refresh_app_card();
        return;
    }

    if (abs((int)delta_x) <= TAP_THRESHOLD_PX) {
        open_current_app();
    }
}

static void create_home_screen(void)
{
    s_home_screen = lv_obj_create(NULL);
    lv_obj_set_flag(s_home_screen, LV_OBJ_FLAG_SCROLLABLE, false);
    set_screen_black(s_home_screen);
    add_black_backdrop(s_home_screen);

    s_time_label = lv_label_create(s_home_screen);
    lv_obj_align(s_time_label, LV_ALIGN_TOP_MID, 0, 10);
    set_text_white(s_time_label);
    lv_obj_set_style_text_font(s_time_label, LV_FONT_DEFAULT, LV_PART_MAIN);

    s_battery_label = lv_label_create(s_home_screen);
    lv_obj_align(s_battery_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    set_text_white(s_battery_label);
    lv_obj_set_style_text_font(s_battery_label, LV_FONT_DEFAULT, LV_PART_MAIN);

    s_app_card = lv_obj_create(s_home_screen);
    lv_obj_set_size(s_app_card, 170, 150);
    lv_obj_align(s_app_card, LV_ALIGN_CENTER, 0, 8);
    lv_obj_set_flag(s_app_card, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_set_style_bg_opa(s_app_card, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_app_card, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_app_card, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(s_app_card, app_card_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_app_card, app_card_event_cb, LV_EVENT_RELEASED, NULL);

    s_app_icon_label = lv_label_create(s_app_card);
    lv_obj_align(s_app_icon_label, LV_ALIGN_CENTER, 0, -20);
    set_text_white(s_app_icon_label);

    s_app_name_label = lv_label_create(s_app_card);
    lv_obj_align(s_app_name_label, LV_ALIGN_CENTER, 0, 20);
    set_text_white(s_app_name_label);

    s_app_hint_label = lv_label_create(s_home_screen);
    lv_obj_align(s_app_hint_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    set_text_white(s_app_hint_label);

    s_no_apps_label = lv_label_create(s_home_screen);
    lv_label_set_text(s_no_apps_label, "No apps available");
    lv_obj_align(s_no_apps_label, LV_ALIGN_CENTER, 0, 0);
    set_text_white(s_no_apps_label);

    lv_timer_create(status_timer_cb, 1000, NULL);
    update_status_bar();
    refresh_app_card();
}

static void splash_timeout_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    show_home_screen();

    if (s_splash_screen != NULL) {
        lv_obj_del(s_splash_screen);
        s_splash_screen = NULL;
    }
}

static void create_and_show_splash_screen(void)
{
    s_splash_screen = lv_obj_create(NULL);
    lv_obj_set_flag(s_splash_screen, LV_OBJ_FLAG_SCROLLABLE, false);
    set_screen_black(s_splash_screen);
    add_black_backdrop(s_splash_screen);

    s_splash_label = lv_label_create(s_splash_screen);
    lv_label_set_text(s_splash_label, "HELLO");
    lv_obj_center(s_splash_label);
    set_text_white(s_splash_label);
    lv_obj_set_style_text_font(s_splash_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_splash_label, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_anim_t fade_in;
    lv_anim_init(&fade_in);
    lv_anim_set_var(&fade_in, s_splash_label);
    lv_anim_set_values(&fade_in, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&fade_in, 700);
    lv_anim_set_exec_cb(&fade_in, set_text_opa_anim);
    lv_anim_set_path_cb(&fade_in, lv_anim_path_ease_in_out);
    lv_anim_start(&fade_in);

    lv_anim_t fade_out;
    lv_anim_init(&fade_out);
    lv_anim_set_var(&fade_out, s_splash_label);
    lv_anim_set_values(&fade_out, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&fade_out, 700);
    lv_anim_set_delay(&fade_out, 1200);
    lv_anim_set_exec_cb(&fade_out, set_text_opa_anim);
    lv_anim_set_path_cb(&fade_out, lv_anim_path_ease_in_out);
    lv_anim_start(&fade_out);

    lv_scr_load(s_splash_screen);
    lv_timer_create(splash_timeout_cb, 2000, NULL);
}
#endif

bool bike_launcher_ui_init(void)
{
#if LVGL_AVAILABLE
    if (lv_display_get_default() == NULL) {
        ESP_LOGE(TAG, "No LVGL display is registered. Skipping launcher UI init.");
        return false;
    }

    s_app_count = bike_app_get_all(&s_apps);
    s_current_app_index = 0;

    create_home_screen();
    create_and_show_splash_screen();

    ESP_LOGI(TAG, "Launcher initialized with %u app(s)", (unsigned int)s_app_count);
    return true;
#else
    ESP_LOGW(TAG, "LVGL not available. Launcher UI is disabled.");
    return false;
#endif
}

void bike_launcher_ui_tick(void)
{
#if LVGL_AVAILABLE
    /* LVGL timing is driven by esp_lvgl_adapter once display is initialized. */
#endif
}
