#include "app_registry.h"

#include "esp_log.h"

static const char *TAG = "app_registry";

static const bike_app_t *APPS = NULL;
static const size_t APP_COUNT = 0U;

size_t bike_app_get_all(const bike_app_t **apps_out)
{
    if (apps_out == NULL) {
        return 0;
    }

    *apps_out = APPS;
    return APP_COUNT;
}

bool bike_app_open(size_t index)
{
    if (index >= APP_COUNT) {
        ESP_LOGW(TAG, "Attempted to open invalid app index: %u", (unsigned int)index);
        return false;
    }

    const bike_app_t *app = &APPS[index];
    if (app->on_open != NULL) {
        app->on_open(app);
    }
    return true;
}
