#include "indication/indication_.hpp"
#include "indication/indication_defs.hpp"

#include "sdkconfig.h" // Configuration variables

#include "esp_check.h"

SemaphoreHandle_t semIndEntry = NULL;
SemaphoreHandle_t semIndRouteLock = NULL;

/* External Variables */
extern SemaphoreHandle_t semSysEntry;
extern SemaphoreHandle_t semNVSEntry;

Indication::Indication(uint8_t myMajorVer, uint8_t myMinorVer, uint8_t myRevNumber)
{
    majorVer = myMajorVer; // We pass in the software version of the project so out LEDs
    minorVer = myMinorVer; // can flash this out during startup
    revNumber = myRevNumber;

    if (xSemaphoreTake(semSysEntry, portMAX_DELAY)) // Get everything from the system that we need.
    {
        if (sys == nullptr)
            sys = System::getInstance();
        xSemaphoreGive(semSysEntry);
    }

    setShowFlags();                // Static enabling of logging statements for any area of concern during development.
    setLogLevels();                // Manually sets log levels for tasks down the call stack for development.
    createSemaphores();            // Create any locking semaphores owned by this object.
    restoreVariablesFromNVS();     // Bring back all our persistant data.
    setConditionalCompVariables(); // Converts all conditional compilation to varibles.

    resetIndication();

    initINDStep = IND_INIT::Start; // Allow the object to initialize.
    indOP = IND_OP::Init;

    xTaskCreate(runMarshaller, "ind_run", 1024 * runStackSizeK, this, 5, &taskHandleRun); // Low number indicates low priority task
}

Indication::~Indication()
{
    if (queHandleINDCmdRequest != nullptr)
        vQueueDelete(queHandleINDCmdRequest);

    if (semIndEntry != NULL)
        vSemaphoreDelete(semIndEntry);

    if (this->taskHandleRun != nullptr)
        vTaskDelete(NULL);
}

void Indication::setShowFlags()
{
    show = 0;
    // show |= _showInit;
    // show |= _showNVS;
    // show |= _showRun;
}

void Indication::setLogLevels()
{
    if (show > 0)                             // Normally, we are interested in the variables inside our object.
        esp_log_level_set(TAG, ESP_LOG_INFO); // If we have any flags set, we need to be sure to turn on the logging so we can see them.
    else
        esp_log_level_set(TAG, ESP_LOG_ERROR); // Likewise, we turn off logging if we are not looking for anything.

    // If we notice any supporting libraries printing to the serial output, we can alter their logging levels here in development.
}

void Indication::createSemaphores()
{
    if (semIndEntry == NULL)
        vSemaphoreCreateBinary(semIndEntry); // External access semaphore

    if (semIndEntry != NULL)
    {
        xSemaphoreGive(semIndEntry);
        xSemaphoreTake(semIndEntry, portMAX_DELAY); // Take the semaphore.  This gives us a locking mechanism for initialization.
    }

    semIndRouteLock = xSemaphoreCreateBinary();
    if (semIndRouteLock != NULL)
        xSemaphoreGive(semIndRouteLock);
}

void Indication::setConditionalCompVariables()
{
}

TaskHandle_t &Indication::getRunTaskHandle(void)
{
    return taskHandleRun;
}

QueueHandle_t &Indication::getCmdRequestQueue(void)
{
    return queHandleINDCmdRequest;
}

//
// This is a 2 Color Sequencer service.  It is  used to deliver 1 or 2 numbers with "blinks" of color.
// Colors may be of a single color or composed of several colors (a combination color).
//
// The assumption is that we have a 4 possible LED indicators, ColorA through ColorD.  Any combination of those 4 colors create 1
// combination color.
//
// We may sequence 2 combination (or single) colors to deliver a two number message through flashing the LEDs.   We may flash any color combination in
// the sequence up to 13 pulses.   We may also choose to flash only one color (or color combination) for up to 13 cycles.
//
//
// The 32 bit Task Notification number we receive encodes an full LED indication sequence
//
//
// byte 1 <color1/cycles>  byte 2 <color1/cycles>  byte 3 <color time-out (time on)>  byte 4 <dark delay (time off)>
//
// 1st byte format for FirstColor is   MSBit    0x<Colors><Cycles>	LSBit
// <Colors>   0x1 = ColorA, 0x2 = ColorB, 0x4 = ColorC, 0x8 = ColorD (4 bits in use here)
// <Cycles>   13 possible flashes - 0x01 though 0x0E (1 through 13) Special Command Codes: 0x00 = ON State, 0x0E = AUTO State, 0x0F = OFF State (4 bits in use here)
//
// NOTE: Special Command codes apply to the states of all LEDs in a combination color.
//
// Second byte is the Second Color/Cycles.
// Third byte is on_time (on-time) -- how long the color is on
// Fourth byte is off_time (off-time)  -- the time the LEDs are off between cycles.
//
// The values of on_time and off_time are shared between both possible color sequences.
//
// Examples:
//  ColorA  Cycles  ColorB   Cycles   On-Time   Off-Time
//  0x1     1       0x2      2        0x20      0x30
//  0x11222030
//
//  ColorC  Cycles  NoColor  Cycles   On-Time   Off-Time
//  0x4     3       0        0        0x01      0x15
//  0x43000115
//
//  All Colors Cycles  NoColor Cycles On-Time  Off-Time -- Effectively takes all colors and turns them ON continously.
//  0x7        0       0       0      0        0
//
//  All Colors Cycles  NoColor Cycles On-Time  Off-Time -- Effectively takes all colors and turns them OFF completely.
//  0x7        F       0       0      0        0
//
//
//
// PLEASE CALL ON THIS SERVICE LIKE THIS:
//
// const int32_t val = 0x22820919;
// xQueueSendToBack(queHandleINDCmdRequest, &val, 30);
//
//
//
esp_err_t Indication::startIndication(uint32_t value)
{
    esp_err_t ret = ESP_OK;
    //  std::cout << "IO Value: " << (uint32_t *)value << std::endl;
    first_color_target = (0xF0000000 & value) >> 28; // First Color(s) -- We may see any of the color bits set
    first_color_cycles = (0x0F000000 & value) >> 24; // First Color Cycles

    second_color_target = (0x00F00000 & value) >> 20; // Second Color(s)
    second_color_cycles = (0x000F0000 & value) >> 16; // Second Color Cycles

    on_time = (0x0000FF00 & value) >> 8; // Time out
    off_time = (0x000000FF & value);         // Dark Time

    first_on_time_counter = on_time;
    second_on_time_counter = on_time;

    // ESP_LOGI(TAG, "First Color 0x%0X Cycles 0x%0X", first_color_target, first_color_cycles);
    // ESP_LOGI(TAG, "Second Color 0x%0X Cycles 0x%0X", second_color_target, second_color_cycles);
    // ESP_LOGI(TAG, "Color Time 0x%02X", on_time);
    // ESP_LOGI(TAG, "Dark Time 0x%02X", off_time);

    //
    // Manually turning ON and OFF or AUTO -- the led LEDs happens always in the First Color byte
    //
    if (first_color_cycles == 0x00) // Turn Colors On and exit routine
    {
        ESP_LOGW(TAG, "first_color_cycles == 0x00");

        if (first_color_target & COLORA_Bit)
            aState = LED_STATE::ON;

        if (first_color_target & COLORB_Bit)
            bState = LED_STATE::ON;

        if (first_color_target & COLORC_Bit)
            cState = LED_STATE::ON;

        clearLEDTargets = 0;
        setLEDTargets = (uint8_t)COLORA_Bit | (uint8_t)COLORB_Bit | (uint8_t)COLORC_Bit;
        setAndClearColors(setLEDTargets, clearLEDTargets);
    }
    else if (first_color_cycles == 0x0F) // Turn Colors Off and exit routine
    {
        ESP_LOGW(TAG, "first_color_cycles == 0x0F");

        if (first_color_target & COLORA_Bit)
            aState = LED_STATE::OFF;

        if (first_color_target & COLORB_Bit)
            bState = LED_STATE::OFF;

        if (first_color_target & COLORC_Bit)
            cState = LED_STATE::OFF;

        setLEDTargets = 0;
        clearLEDTargets = (uint8_t)COLORA_Bit | (uint8_t)COLORB_Bit | (uint8_t)COLORC_Bit;
        setAndClearColors(setLEDTargets, clearLEDTargets);
    }
    else if (first_color_cycles == 0x0E) // Turn Colors to AUTO State and exit routine
    {
        ESP_LOGW(TAG, "first_color_cycles == 0x0E");

        if (first_color_target & COLORA_Bit)
            aState = LED_STATE::AUTO;

        if (first_color_target & COLORB_Bit)
            bState = LED_STATE::AUTO;

        if (first_color_target & COLORC_Bit)
            cState = LED_STATE::AUTO;
        setAndClearColors(first_color_target, 0);
    }
    else
    {
        setAndClearColors(first_color_target, 0); // Process normal color display
        first_color_cycles--;
        indStates = IND_STATES::Show_FirstColor;
        IsIndicating = true;
    }
    return ret;
}

void Indication::setAndClearColors(uint8_t SetColors, uint8_t ClearColors)
{
    esp_err_t ret = ESP_OK;
    uint8_t led_strip_pixels[3];

    led_strip_pixels[0] = bCurrValue; // Green
    led_strip_pixels[1] = aCurrValue; // Red
    led_strip_pixels[2] = cCurrValue; // Blue

    //
    // ESP_LOGI(TAG, "SetAndClearColors 0x%02X 0x%02X", SetColors, ClearColors); // Debug info
    // ESP_LOGI(TAG, "Colors Curr/Default  %d/%d   %d/%d   %d/%d", aCurrValue, aDefaultValue, bCurrValue, bDefaultValue, cCurrValue, cDefaultValue);
    //
    if (ClearColors & COLORA_Bit) // Seeing the bit to Clear this color
    {
        if (aState == LED_STATE::ON)
            aCurrValue = aDefaultValue; // Don't turn off this value because our stat is ON
        else
            aCurrValue = 0; // Otherwise, do turn it off.

        led_strip_pixels[1] = aCurrValue; // Red
    }

    if (ClearColors & COLORB_Bit)
    {
        if (bState == LED_STATE::ON)
            bCurrValue = bDefaultValue;
        else
            bCurrValue = 0;

        led_strip_pixels[0] = bCurrValue; // Green
    }

    if (ClearColors & COLORC_Bit)
    {
        if (cState == LED_STATE::ON)
            cCurrValue = cDefaultValue;
        else
            cCurrValue = 0;

        led_strip_pixels[2] = cCurrValue; // Blue
    }

    if (SetColors & COLORA_Bit) // Setting the bit to Set this color
    {
        if (aState == LED_STATE::OFF) // Set the color if the state is set above OFF
            aCurrValue = 0;           // Don't allow any value to be displayed on the LED
        else
            aCurrValue = aDefaultValue; // State is either AUTO or ON.

        led_strip_pixels[1] = aCurrValue; // Red
    }

    if (SetColors & COLORB_Bit)
    {
        if (bState == LED_STATE::OFF)
            bCurrValue = 0;
        else
            bCurrValue = bDefaultValue;

        led_strip_pixels[0] = bCurrValue; // Green
    }

    if (SetColors & COLORC_Bit)
    {
        if (cState == LED_STATE::OFF)
            cCurrValue = 0;
        else
            cCurrValue = cDefaultValue;

        led_strip_pixels[2] = cCurrValue; // Blue
    }

    // ESP_LOGW(TAG, "Red   State/Value  %d/%d", (int)aState, aCurrValue);
    // ESP_LOGW(TAG, "Green State/Value  %d/%d", (int)bState, bCurrValue);
    // ESP_LOGW(TAG, "Blue  State/Value  %d/%d", (int)cState, cCurrValue);
    // ESP_LOGW(TAG, "---------------------------------------------------");

    ret = rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
    ret = rmt_tx_wait_all_done(led_chan, portMAX_DELAY);

    if (ret != ESP_OK)
        ESP_LOGE(TAG, "::%s %s", __func__, esp_err_to_name(ret));
}

void Indication::resetIndication()
{
    first_color_target = 0;
    first_color_cycles = 0;

    second_color_target = 0;
    second_color_cycles = 0;

    off_time = 0;
    off_time_counter = 0;

    on_time = 0;
    on_time_counter = 0;

    indStates = IND_STATES::Init;
    IsIndicating = false;
}

/* LED Strip Routines */
esp_err_t Indication::rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;

    if (config == nullptr)
    {
        ESP_LOGE(TAG, "config encoder parameter can not be null...");
        return ESP_FAIL;
    }

    if unlikely (ret_encoder == nullptr)
    {
        ESP_LOGE(TAG, "ret_encoder handle parameter can not be null...");
        return ESP_FAIL;
    }

    rmt_led_strip_encoder_t *led_encoder = (rmt_led_strip_encoder_t *)calloc(1, sizeof(rmt_led_strip_encoder_t));

    if unlikely (led_encoder == nullptr)
    {
        ESP_LOGE(TAG, "Memory for rmt_led_strip_encoder_t allocation failed...");
        return ESP_FAIL;
    }

    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;

    // different led strip might have its own timing requirements, following parameter is for WS2812

    uint16_t bit0_duration0 = 0.3 * config->resolution / 1000000; // T1H=0.9us
    uint16_t bit0_duration1 = 0.9 * config->resolution / 1000000; // T0L=0.9us
    uint16_t bit1_duration0 = 0.9 * config->resolution / 1000000; // T1H=0.9us
    uint16_t bit1_duration1 = 0.3 * config->resolution / 1000000; // T1L=0.3us

    rmt_bytes_encoder_config_t bytes_encoder_config = {
        {bit0_duration0, 1, bit0_duration1, 0}, // bit0_duration0, bit0_level0, bit0_duration1, bit0_level1
        {bit1_duration0, 1, bit1_duration1, 0}, // bit1_duration0, bit1_level0, bit1_duration1, bit1_level1
        1,                                      // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder);

    if unlikely (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s::rmt_new_bytes_encoder() failed.  err = %d - %s", __func__, ret, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    rmt_copy_encoder_config_t copy_encoder_config = {};

    ret = rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder);

    if unlikely (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s::rmt_new_copy_encoder() failed.  err = %d - %s", __func__, ret, esp_err_to_name(ret));

        if (led_encoder->bytes_encoder) // Clean up the encoder from previous area
            rmt_del_encoder(led_encoder->bytes_encoder);

        return ESP_FAIL;
    }
    uint32_t reset_ticks = config->resolution / 1000000 * 50 / 2; // reset code duration defaults to 50us

    led_encoder->reset_code = (rmt_symbol_word_t){
        {(uint16_t)reset_ticks, 0, (uint16_t)reset_ticks, 0},
    };
    *ret_encoder = &led_encoder->base;

    return ESP_OK;
}

size_t Indication::rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (led_encoder->state)
    {
    case 0:
    {
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE)
        {
            led_encoder->state = 1; // switch to next state when current encoding session finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL)
        {
            state = (rmt_encode_state_t)(state | RMT_ENCODING_MEM_FULL);
            break; // yield if there's no free space for encoding artifacts
        }
        [[fallthrough]];
    }

    case 1:
    {
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code, sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE)
        {
            led_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
            state = (rmt_encode_state_t)(state | RMT_ENCODING_COMPLETE);
        }
        if (session_state & RMT_ENCODING_MEM_FULL)
        {
            state = (rmt_encode_state_t)(state | RMT_ENCODING_MEM_FULL);
            break; // yield if there's no free space for encoding artifacts
        }
        break;
    }
    }

    *ret_state = state;
    return encoded_symbols;
}

esp_err_t Indication::rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    ESP_RETURN_ON_ERROR(rmt_del_encoder(led_encoder->bytes_encoder), "IND", "rmt_del_encoder() failed.");
    ESP_RETURN_ON_ERROR(rmt_del_encoder(led_encoder->copy_encoder), "IND", "rmt_del_encoder() failed.");
    free(led_encoder);
    return ESP_OK;
}

esp_err_t Indication::rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    ESP_RETURN_ON_ERROR(rmt_encoder_reset(led_encoder->bytes_encoder), "IND", "rmt_encoder_reset() failed.");
    ESP_RETURN_ON_ERROR(rmt_encoder_reset(led_encoder->copy_encoder), "IND", "rmt_encoder_reset() failed.");
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

/* Untilities */
std::string Indication::getStateText(uint8_t colorState)
{
    switch (colorState)
    {
    case 0:
        return "NONE";
    case 1:
        return "OFF";
    case 2:
        return "AUTO";
    case 3:
        return "ON";
    default:
        return "UNKNOWN";
    }
}
