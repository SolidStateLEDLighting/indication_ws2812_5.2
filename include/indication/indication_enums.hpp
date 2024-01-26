#pragma once

#include <stdint.h>

#include "driver/rmt_types.h"
#include "driver/rmt_tx.h"

typedef struct
{
    uint32_t resolution; /*!< Encoder resolution, in Hz */
} led_strip_encoder_config_t;

typedef struct
{
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

enum class IND_NOTIFY : uint32_t // Task Notification definitions for the Run loop
{
    NONE = 0,
    SET_A_COLOR_BRIGHTNESS = 256,  // Lower byte holeds 8 bit brightness value
    SET_B_COLOR_BRIGHTNESS = 512,  //
    SET_C_COLOR_BRIGHTNESS = 1024, //
    CMD_SHUT_DOWN = 4096,          // We are slipping a command into our notification schema
};

//
// Class Operations
//
enum LED_BITS
{
    COLORA_Bit = 0x01,
    COLORB_Bit = 0x02,
    COLORC_Bit = 0x04,
};

enum LED_TRI_COLOR
{
    Red = 0x01,
    Green = 0x02,
    Yellow = 0x03,
    Blue = 0x04,
    Purple = 0x05,
    Cyan = 0x06,
    White = 0x07,
};

enum class LED_STATE : uint8_t
{
    NONE, // Zero value here helps catch an error of empty value in nvs
    OFF,
    AUTO,
    ON,
};

enum class IND_OP : uint8_t // Primary Operations
{
    Run,
    Shutdown,
    Init,
    Error,
    Idle,
    Idle_Silent,
};

enum class IND_SHUTDOWN : uint8_t
{
    Start,
    DisableAndDeleteRMTChannel,
    Final_Items,
    Finished,
};

enum class IND_INIT : uint8_t
{
    Start,
    Init_Queues_Commands,
    CreateRMTTxChannel,
    CreateRMTEncoder,
    EnableRMTChannel,
    Set_LED_Initial_States,
    Early_Release,
    ColorA_On,
    ColorA_Off,
    ColorB_On,
    ColorB_Off,
    ColorC_On,
    ColorC_Off,
    Finished,
};

enum class IND_STATES : uint8_t
{
    Idle,
    Show_FirstColor,
    FirstColor_Dark,
    Show_SecondColor,
    SecondColor_Dark,
    Final,
};
