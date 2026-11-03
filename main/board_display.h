#pragma once

#include <stdbool.h>
#include <stdint.h>

bool bike_board_display_init(void);
bool bike_board_display_show(void);
bool bike_board_display_lock(uint32_t timeout_ms);
void bike_board_display_unlock(void);
