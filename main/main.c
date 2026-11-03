#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_display.h"
#include "launcher_ui.h"
#include "ui_config.h"

static const char *TAG = "bike_nav";

static void lvgl_ui_task(void *arg)
{
    (void)arg;

#if LVGL_AVAILABLE
    if (!bike_board_display_init()) {
        ESP_LOGE(TAG, "Display/touch initialization failed.");
    } else if (!bike_board_display_lock(portMAX_DELAY)) {
        ESP_LOGE(TAG, "Failed to lock LVGL for launcher UI setup.");
    } else if (bike_launcher_ui_init()) {
        bike_board_display_unlock();
        if (!bike_board_display_show()) {
            ESP_LOGW(TAG, "Initial LVGL frame show failed.");
        }
        ESP_LOGI(TAG, "LVGL launcher UI created.");
    } else {
        bike_board_display_unlock();
        ESP_LOGE(TAG, "LVGL initialized but launcher UI was not created.");
    }
#else
    ESP_LOGW(TAG, "LVGL not available. Running without UI.");
#endif

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
        bike_launcher_ui_tick();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Bike navigation firmware booting...");
    ESP_LOGI(TAG, "Initializing system services and UI foundation.");

    BaseType_t status = xTaskCreatePinnedToCore(
        lvgl_ui_task,
        "lvgl_ui_task",
        4096,
        NULL,
        5,
        NULL,
        1
    );

    if (status != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL UI task.");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Application started. Awaiting UI and sensor tasks.");
}
