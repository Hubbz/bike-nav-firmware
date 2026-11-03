#include "system_status.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void bike_status_get_time_text(char *buffer, size_t buffer_len)
{
    if (buffer == NULL || buffer_len == 0U) {
        return;
    }

    const time_t now = time(NULL);
    struct tm local_time;
    memset(&local_time, 0, sizeof(local_time));

    if (now <= 0 || localtime_r(&now, &local_time) == NULL) {
        snprintf(buffer, buffer_len, "--:--");
        return;
    }

    strftime(buffer, buffer_len, "%H:%M", &local_time);
}

uint8_t bike_status_get_battery_percent(void)
{
    return 100U;
}
