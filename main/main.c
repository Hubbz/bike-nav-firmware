#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if __has_include("lvgl.h")
#include "lvgl.h"
#define LVGL_AVAILABLE 1
#else
#define LVGL_AVAILABLE 0
#endif

static const char *TAG = "bike_nav";

static void lvgl_ui_task(void *arg)
{
    (void)arg;

#if LVGL_AVAILABLE
    lv_init();

    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title_label = lv_label_create(screen);
    lv_label_set_text(title_label, "Bike Nav");
    lv_obj_center(title_label);

    ESP_LOGI(TAG, "LVGL initialized and UI foundation created.");
#endif

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));

#if LVGL_AVAILABLE
        lv_timer_handler();
#endif
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
