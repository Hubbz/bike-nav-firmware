#include "app_registry.h"
#include "apps/alert/alert_app.h"
#include "apps/security/security_app.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "app_registry";

static void app_layout_back_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *screen = (lv_obj_t *)lv_event_get_user_data(event);
    if (screen == NULL) {
        return;
    }

    lv_obj_send_event(screen, LV_EVENT_RELEASED, NULL);
}

void bike_app_layout_init(lv_obj_t *screen, const char *title_text, bike_app_layout_t *layout)
{
    if (screen == NULL || layout == NULL) {
        return;
    }

    memset(layout, 0, sizeof(*layout));

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, title_text ? title_text : "");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, BIKE_LAYOUT_TOP_Y);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, LV_PART_MAIN);
    layout->title = title;

    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_center(content);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(content, false);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_CLICKABLE);
    layout->content = content;

    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, "");
    lv_obj_set_size(footer, LV_PCT(100), 22);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, BIKE_LAYOUT_BOTTOM_Y);
    lv_obj_set_style_text_color(footer, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    layout->footer = footer;

    lv_obj_t *back_button = lv_label_create(screen);
    lv_label_set_text(back_button, "< Back");
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_MID, 0, BIKE_LAYOUT_BOTTOM_Y);
    lv_obj_set_style_text_color(back_button, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(back_button, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_pad_left(back_button, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(back_button, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(back_button, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(back_button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back_button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(back_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back_button, app_layout_back_cb, LV_EVENT_CLICKED, screen);
    layout->back_button = back_button;
}

static const bike_app_t APPS[] = {
    {"security", "Security", "🔒", bike_arm_alarm_app_open},
    {"alert", "Alert", "🚨", bike_alert_app_open},
};

static const size_t APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);

size_t bike_app_get_all(const bike_app_t **apps_out)
{
    if (apps_out == NULL) {
        return 0;
    }

    *apps_out = APPS;
    return APP_COUNT;
}

bool bike_app_open(size_t index, lv_obj_t *screen)
{
    if (index >= APP_COUNT) {
        ESP_LOGW(TAG, "Attempted to open invalid app index: %u", (unsigned int)index);
        return false;
    }

    const bike_app_t *app = &APPS[index];
    if (app->on_open != NULL) {
        app->on_open(app, screen);
        return true;
    }
    return false;
}
