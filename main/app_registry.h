#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"

#define BIKE_LAYOUT_TOP_Y 20
#define BIKE_LAYOUT_BOTTOM_Y -20
#define BIKE_LAYOUT_CARD_WIDTH 170
#define BIKE_LAYOUT_CARD_HEIGHT 150
#define BIKE_LAYOUT_CARD_CENTER_Y 8
#define BIKE_LAYOUT_CARD_ICON_Y -20
#define BIKE_LAYOUT_CARD_TEXT_Y 20

typedef struct {
    lv_obj_t *title;
    lv_obj_t *content;
    lv_obj_t *footer;
    lv_obj_t *back_button;
} bike_app_layout_t;

typedef struct bike_app {
    const char *id;
    const char *name;
    const char *icon;
    void (*on_open)(const struct bike_app *app, lv_obj_t *screen);
} bike_app_t;

void bike_app_layout_init(lv_obj_t *screen, const char *title_text, bike_app_layout_t *layout);
size_t bike_app_get_all(const bike_app_t **apps_out);
bool bike_app_open(size_t index, lv_obj_t *screen);
