#pragma once

#include <stddef.h>
#include <stdint.h>

void bike_status_get_time_text(char *buffer, size_t buffer_len);
uint8_t bike_status_get_battery_percent(void);
