#pragma once

#include "indication/indication_enums.hpp"
#include "sdkconfig.h"
#include "system_defs.hpp"

#ifndef CONFIG_WS2812_LED_ENABLE
#define ENABLE_INDICATION 0
#define RMT_LED_STRIP_GPIO_NUM 0
#else
#define ENABLE_INDICATION 1
#define RMT_LED_STRIP_GPIO_NUM CONFIG_WS2812_LED_GPIO // GPIO 48 for the ESP32-S3 built-in addressable LED
#endif

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
