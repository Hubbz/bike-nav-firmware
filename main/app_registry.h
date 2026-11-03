#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct bike_app {
    const char *id;
    const char *name;
    const char *icon;
    void (*on_open)(const struct bike_app *app);
} bike_app_t;

size_t bike_app_get_all(const bike_app_t **apps_out);
bool bike_app_open(size_t index);
