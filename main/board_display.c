#include "board_display.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"

static const char *TAG = "board_display";

static bool s_initialized = false;

bool bike_board_display_init(void)
{
    if (s_initialized) {
        return true;
    }

    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "Display start failed");
        return false;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Display and touch initialized via Waveshare BSP");
    return true;
}

bool bike_board_display_show(void)
{
    if (bsp_display_show() != ESP_OK) {
        return false;
    }

    if (bsp_display_brightness_set(100) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to force display brightness to 100%%");
    }

    return true;
}

bool bike_board_display_lock(uint32_t timeout_ms)
{
    return bsp_display_lock(timeout_ms);
}

void bike_board_display_unlock(void)
{
    bsp_display_unlock();
}
