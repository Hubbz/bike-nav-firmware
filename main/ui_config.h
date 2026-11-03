#pragma once

#if __has_include("lvgl.h")
#include "lvgl.h"
#define LVGL_AVAILABLE 1
#else
#define LVGL_AVAILABLE 0
#endif
