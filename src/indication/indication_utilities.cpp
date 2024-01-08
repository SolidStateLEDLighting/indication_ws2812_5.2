#include "indication/indication_.hpp"

/* Untilities */
std::string Indication::getStateText(LED_STATE colorState)
{
    switch (colorState)
    {
    case LED_STATE::NONE:
        return "NONE";
    case LED_STATE::OFF:
        return "OFF";
    case LED_STATE::AUTO:
        return "AUTO";
    case LED_STATE::ON:
        return "ON";
    default:
        return "UNKNOWN";
    }
}
