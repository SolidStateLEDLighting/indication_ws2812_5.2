#pragma once

#include "indication/indication_enums.hpp"
#include "sdkconfig.h"
#include "system_defs.hpp"

#define RMT_LED_STRIP_GPIO_NUM CONFIG_WS2812_LED_GPIO // Set the GPIO from Kconfig.  Default are provided for some DevKitC hardware
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000          // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

#define _showINDShdnSteps 0x01