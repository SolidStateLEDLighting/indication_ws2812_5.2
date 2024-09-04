/** YOU MUST VIEW THIS PROJECT IN VS CODE TO SEE FOLDING AND THE PERFECTION OF THE DESIGN **/

#include "indication/indication_.hpp"

#include "driver/rmt_tx.h"

/* External Semaphores */
extern SemaphoreHandle_t semIndEntry;

void Indication::runMarshaller(void *arg)
{
    ((Indication *)arg)->run();
    ((Indication *)arg)->taskHandleRun = nullptr; // This doesn't happen automatically but we look at this variable for validity, so set it manually.
    vTaskDelete(NULL);
}

void Indication::run(void)
{
    esp_err_t ret = ESP_OK;

    uint8_t cycles = 0;
    uint32_t value = 0;

    TickType_t startDwellTime = 0;

    // We want our dwell time to be about 10mSecs.  When the RTOS tick rate is 100Hz, our dwellTime ticks are only 1.
    // If we should increase our tick rate upward, we will want to automatically calculate a new tick dwellTime.
    TickType_t dwellTime = pdMS_TO_TICKS(10);
    // ESP_LOGW(TAG, "dwellTime is %ldmSec with %d ticks", pdTICKS_TO_MS(dwellTime), dwellTime); // Verfication during development only

    resetIndication();

    while (true)
    {
        switch (indOP)
        {
        case IND_OP::Run: // When NOT indicating, we would like to achieve about a 4Hz entry cadence in the Run state.
        {
            if (IsIndicating) // The priority is the do the indication.  We can only perform one indication at a time.
            {
                startDwellTime = xTaskGetTickCount();
                xTaskDelayUntil(&startDwellTime, dwellTime);

                switch (indState)
                {
                case IND_STATES::Idle:
                {
                    break;
                }

                case IND_STATES::Clear_FirstColor:
                {
                    if ((first_on_time_counter > 0) && (--first_on_time_counter < 1)) // First color on-time just expired
                    {
                        setAndClearColors(0, first_color_target); // Turn off the first color
                        indState = IND_STATES::Set_FirstColor;    // Assume that a first color has another cycle by default

                        if (first_color_cycles < 1)
                        {
                            if (second_color_target < 1)
                                off_time_counter = 3 * off_time; // IF we are finished with the first color AND we don't have a second color THEN add extra off delay time.
                            else
                            {
                                off_time_counter = 2 * off_time; // Moving to second color off delay time.
                                indState = IND_STATES::Set_SecondColor;
                            }
                        }
                        else
                            off_time_counter = off_time; // Normal off delay time between color one cycles
                    }
                    break;
                }

                case IND_STATES::Clear_SecondColor:
                {
                    if ((second_on_time_counter > 0) && (--second_on_time_counter < 1)) // Second color on-time just expired
                    {
                        setAndClearColors(0, second_color_target); // Turn off the second color
                        indState = IND_STATES::Set_SecondColor;    // Assume that a second color has another cycle by default

                        if (second_color_cycles < 1)         // If we are finished with the second color -- add extra delay time
                            off_time_counter = 3 * off_time; // If we are finish then add extra delay time between this code and one that might come next.
                        else
                            off_time_counter = off_time; // Normal off-time.
                    }
                    break;
                }

                case IND_STATES::Set_FirstColor:
                case IND_STATES::Set_SecondColor:
                {
                    if ((off_time_counter > 0) && (--off_time_counter < 1))
                    {
                        if ((first_color_cycles > 0) && (indState == IND_STATES::Set_FirstColor))
                        {
                            first_color_cycles--;
                            setAndClearColors(first_color_target, 0); // Turn on the LED
                            first_on_time_counter = on_time;
                            indState = IND_STATES::Clear_FirstColor;
                        }
                        else if ((second_color_cycles > 0) && (indState == IND_STATES::Set_SecondColor))
                        {
                            second_color_cycles--;
                            setAndClearColors(second_color_target, 0); // Turn on the LED
                            second_on_time_counter = on_time;
                            indState = IND_STATES::Clear_SecondColor;
                        }
                        else
                        {
                            indState = IND_STATES::Final;
                        }
                    }
                    break;
                }

                case IND_STATES::Final:
                {
                    if (rmtEstablished)
                        ESP_GOTO_ON_ERROR(demolishRMTDriver(), ind_final_err, TAG, "demolishRMTDriver() failed");
                    resetIndication(); // Resetting all the indicator variables
                    break;

                ind_final_err:
                    errMsg = std::string(__func__) + "(): " + esp_err_to_name(ret);
                    indOP = IND_OP::Error;
                    break;
                }
                }
            }
            else // When we are not indicating -- we are looking for notifications or incoming commands.
            {
                indTaskNotifyValue = static_cast<IND_NOTIFY>(ulTaskNotifyTake(pdTRUE, 0)); // This is a rare command so we don't wait here.

                if (indTaskNotifyValue > static_cast<IND_NOTIFY>(0))
                {
                    // ESP_LOGW(TAG, "Task notification Colors 0x%02X  Value is %d", ((((int)indTaskNotifyValue) & 0xFFFFFF00) >> 8), (int)indTaskNotifyValue & 0x000000FF);

                    if ((int)indTaskNotifyValue & (int)IND_NOTIFY::NFY_SET_A_COLOR_BRIGHTNESS)
                    {
                        aSetLevel = (int)indTaskNotifyValue & 0x000000FF;
                        startNVSDelayTicks = xTaskGetTickCount();
                    }
                    else if ((int)indTaskNotifyValue & (int)IND_NOTIFY::NFY_SET_B_COLOR_BRIGHTNESS)
                    {
                        bSetLevel = (int)indTaskNotifyValue & 0x000000FF;
                        startNVSDelayTicks = xTaskGetTickCount();
                    }
                    else if ((int)indTaskNotifyValue & (int)IND_NOTIFY::NFY_SET_C_COLOR_BRIGHTNESS)
                    {
                        cSetLevel = (int)indTaskNotifyValue & 0x000000FF;
                        startNVSDelayTicks = xTaskGetTickCount();
                    }
                    else if ((int)indTaskNotifyValue & (int)IND_NOTIFY::CMD_SHUT_DOWN)
                    {
                        indShdnStep = IND_SHUTDOWN::Start;
                        indOP = IND_OP::Shutdown;
                        break;
                    }
                    else
                        routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unhandled TaskNotification");
                }
                else if (xQueueReceive(queHandleIndCmdRequest, (void *)&value, pdMS_TO_TICKS(245)) == pdTRUE) // We can wait here most of the time for requests
                {
                    // ESP_LOGW(TAG, "Received notification value of %08X", (int)value);
                    startIndication(value); // We have an indication value
                }
            }

            // Even if we are indicating, we may want to perform some background work.  We can do that here.

            if (startNVSDelayTicks > 0) // If we in the process of counting time (ticks)
            {
                if (xTaskGetTickCount() > (startNVSDelayTicks + mSecNVSDelayTicks))
                {
                    saveVariablesToNVS();
                    routeLogByValue(LOG_TYPE::WARN, std::string(__func__) + "(): saveVariablesToNVS()");
                    startNVSDelayTicks = 0; // Stop the count for NVS storage
                }
            }
            break;
        }

        case IND_OP::Shutdown:
        {
            // Positionally, it is important for Shutdown to be serviced right after it is called.  We don't want other possible operations becoming active unexepectedly.
            // A shutdown is a complete undoing of all items that were established or created with our run thread.
            // If we are connected then disconnect.  If we created resources (after construction), we must dispose of them here.
            switch (indShdnStep)
            {
            case IND_SHUTDOWN::Start:
            {
                if (showIND & _showINDShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_SHUTDOWN::Start");

                if (rmtEstablished)
                    indShdnStep = IND_SHUTDOWN::DisableAndDeleteRMTChannel;
                else
                    indShdnStep = IND_SHUTDOWN::Final_Items;
                break;
            }

            case IND_SHUTDOWN::DisableAndDeleteRMTChannel:
            {
                if (showIND & _showINDShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_SHUTDOWN::DisableAndDeleteRMTChannel - Step " + std::to_string((int)IND_SHUTDOWN::DisableAndDeleteRMTChannel));

                if (rmtEstablished)
                    ESP_GOTO_ON_ERROR(demolishRMTDriver(), ind_disableAndDeleteRMTChannel_err, TAG, "demolishRMTDriver() failed");
                indShdnStep = IND_SHUTDOWN::Final_Items;
                break;

            ind_disableAndDeleteRMTChannel_err:
                errMsg = std::string(__func__) + "(): " + esp_err_to_name(ret);
                indOP = IND_OP::Error;
                break;
            }

            case IND_SHUTDOWN::Final_Items:
            {
                if (showIND & _showINDShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_SHUTDOWN::Final_Items - Step " + std::to_string((int)IND_SHUTDOWN::Final_Items));

                indShdnStep = IND_SHUTDOWN::Finished;
                break;
            }

            case IND_SHUTDOWN::Finished:
            {
                if (showIND & _showINDShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_SHUTDOWN::Finished");
                return; // This exits the run function. (notice how the compiler doesn't complain about the missing break statement)
            }
            }
            break;
        }

        case IND_OP::Init:
        {
            switch (indInitStep)
            {
            case IND_INIT::Start:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::Start");

                indInitStep = IND_INIT::StartRMTDriver;
                [[fallthrough]];
            }

            case IND_INIT::StartRMTDriver:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::StartRMTDriver - Step " + std::to_string((int)IND_INIT::StartRMTDriver));

                if (!rmtEstablished)
                    ESP_GOTO_ON_ERROR(establishRMTDriver(), ind_startRMTDriver_err, TAG, "establishRMTDriver() failed");
                indInitStep = IND_INIT::Set_LED_Initial_States;
                break;

            ind_startRMTDriver_err:
                errMsg = std::string(__func__) + "(): " + esp_err_to_name(ret);
                indOP = IND_OP::Error;
                break;
            }

            case IND_INIT::Set_LED_Initial_States:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::Set_LED_Initial_States - Step " + std::to_string((int)IND_INIT::Set_LED_Initial_States));

                if (aState == LED_STATE::ON) // Now that the RMT driver has been initialized, we just need to set Color channels according to their States.
                    setAndClearColors(COLORA_Bit, 0);
                else
                    setAndClearColors(0, COLORA_Bit);

                if (bState == LED_STATE::ON)
                    setAndClearColors(COLORB_Bit, 0);
                else
                    setAndClearColors(0, COLORB_Bit);

                if (cState == LED_STATE::ON)
                    setAndClearColors(COLORC_Bit, 0);
                else
                    setAndClearColors(0, COLORC_Bit);

                indInitStep = IND_INIT::Early_Release;
                break;
            }

            case IND_INIT::Early_Release:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::Early_Release - Step " + std::to_string((int)IND_INIT::Early_Release));

                // In the event that no one else is using the LED for any kind of indication during initialization, the object can release it's locking semaphore early.
                xSemaphoreGive(semIndEntry);

                // If any colors are in the On state, or if ALL colors are in the Off state, then bypass the Flashing of the version numbers.
                if ((aState == LED_STATE::ON) || (bState == LED_STATE::ON) || (cState == LED_STATE::ON))
                    indInitStep = IND_INIT::Finished;
                else if ((aState == LED_STATE::OFF) && (bState == LED_STATE::OFF) && (cState == LED_STATE::OFF))
                    indInitStep = IND_INIT::Finished;
                else
                {
                    cycles = majorVer;

                    if (cycles > 0)
                        indInitStep = IND_INIT::ColorA_On;
                    else
                        indInitStep = IND_INIT::ColorA_Off;
                }
                break;
            }

            case IND_INIT::ColorA_On:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorA_On - Step " + std::to_string((int)IND_INIT::ColorA_On));

                cycles--;
                setAndClearColors((uint8_t)COLORA_Bit, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                indInitStep = IND_INIT::ColorA_Off;
                break;
            }

            case IND_INIT::ColorA_Off:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorA_Off - Step " + std::to_string((int)IND_INIT::ColorA_Off));

                setAndClearColors(0, (uint8_t)COLORA_Bit);
                vTaskDelay(pdMS_TO_TICKS(150));

                if (cycles > 0)
                    indInitStep = IND_INIT::ColorA_On;
                else
                {
                    cycles = minorVer;

                    if (cycles > 0)
                        indInitStep = IND_INIT::ColorB_On;
                    else
                        indInitStep = IND_INIT::ColorB_Off;

                    vTaskDelay(pdMS_TO_TICKS(250)); // Off Time
                }
                break;
            }

            case IND_INIT::ColorB_On:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorB_On - Step " + std::to_string((int)IND_INIT::ColorB_On));

                cycles--;
                setAndClearColors((uint8_t)COLORB_Bit, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                indInitStep = IND_INIT::ColorB_Off;
                break;
            }

            case IND_INIT::ColorB_Off:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorB_Off - Step " + std::to_string((int)IND_INIT::ColorB_Off));

                setAndClearColors(0, (uint8_t)COLORB_Bit);
                vTaskDelay(pdMS_TO_TICKS(150));

                if (cycles > 0) //
                    indInitStep = IND_INIT::ColorB_On;
                else
                {
                    cycles = patchNumber;

                    if (cycles > 0)
                        indInitStep = IND_INIT::ColorC_On;
                    else
                        indInitStep = IND_INIT::ColorC_Off;

                    vTaskDelay(pdMS_TO_TICKS(250)); // Off Time
                }
                break;
            }

            case IND_INIT::ColorC_On:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorC_On - Step " + std::to_string((int)IND_INIT::ColorC_On));

                cycles--;
                setAndClearColors((uint8_t)COLORC_Bit, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                indInitStep = IND_INIT::ColorC_Off;
                break;
            }

            case IND_INIT::ColorC_Off:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorC_Off - Step " + std::to_string((int)IND_INIT::ColorC_Off));

                setAndClearColors(0, (uint8_t)COLORC_Bit);
                vTaskDelay(pdMS_TO_TICKS(150));

                if (cycles > 0)
                    indInitStep = IND_INIT::ColorC_On;
                else
                {
                    indInitStep = IND_INIT::StopRMTDriver;
                    vTaskDelay(pdMS_TO_TICKS(250)); // Off Time
                }
                break;
            }

            case IND_INIT::StopRMTDriver:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::StopRMTDriver - Step " + std::to_string((int)IND_INIT::StopRMTDriver));

                if (rmtEstablished)
                    ESP_GOTO_ON_ERROR(demolishRMTDriver(), ind_stopRMTDriver_err, TAG, "demolishRMTDriver() failed");
                indInitStep = IND_INIT::Finished;
                break;

            ind_stopRMTDriver_err:
                errMsg = std::string(__func__) + "(): " + esp_err_to_name(ret);
                indOP = IND_OP::Error;
                break;
            }

            case IND_INIT::Finished:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::Finished");

                indOP = IND_OP::Run;
                xSemaphoreGive(semIndEntry); // Yield now if not done earlier inside Early_Release
                break;
            }
            }
            break;
        }

        case IND_OP::Error:
        {
            routeLogByValue(LOG_TYPE::ERROR, errMsg);
            indOP = IND_OP::Idle;
            break;
        }

        case IND_OP::Idle:
        {
            if (show & _showRun)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_OP::Idle");
            vTaskDelay(pdMS_TO_TICKS(5000));
            break;
        }
        }
        taskYIELD();
    }
}

esp_err_t Indication::establishRMTDriver()
{
    esp_err_t ret = ESP_OK;
    //
    // WARNING: Some processors many NOT support DMA in the configuration below.  If not, just apply a 0 in the correct place.
    //
    rmt_tx_channel_config_t tx_chan_config = {
        (gpio_num_t)RMT_LED_STRIP_GPIO_NUM, // selects GPIO
        RMT_CLK_SRC_DEFAULT,                // selects source clock
        RMT_LED_STRIP_RESOLUTION_HZ,        //
        64,                                 // Increasing the block size can make the LED flicker less
        4,                                  // Set the number of transactions that can be pending in the background
        0,                                  // Interrupt Priority
        {0, 1, 1, 0},                       // invert_out, with_dma, io_loop_back, io_od_mode
    };

    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_chan_config, &led_chan), TAG, "rmt_new_tx_channel() failed");

    led_strip_encoder_config_t encoder_config = {
        RMT_LED_STRIP_RESOLUTION_HZ,
    };

    ESP_RETURN_ON_ERROR(rmt_new_led_strip_encoder(&encoder_config, &led_encoder), TAG, "rmt_new_led_strip_encoder() failed");
    ESP_RETURN_ON_ERROR(rmt_enable(led_chan), TAG, "rmt_enable() failed");
    rmtEstablished = true;
    taskYIELD();
    return ret;
}

esp_err_t Indication::demolishRMTDriver()
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_ERROR(rmt_disable(led_chan), TAG, "rmt_disable() failed");
    ESP_RETURN_ON_ERROR(rmt_del_encoder(led_encoder), TAG, "rmt_del_encoder() failed");
    ESP_RETURN_ON_ERROR(rmt_del_channel(led_chan), TAG, "rmt_del_channel() failed");
    rmtEstablished = false;
    return ret;
}

void Indication::startIndication(uint32_t value)
{
    esp_err_t ret = ESP_OK;

    first_color_target = (0xF0000000 & value) >> 28; // First Color(s) -- We may see any of the color bits set
    first_color_cycles = (0x0F000000 & value) >> 24; // First Color Cycles

    second_color_target = (0x00F00000 & value) >> 20; // Second Color(s)
    second_color_cycles = (0x000F0000 & value) >> 16; // Second Color Cycles

    on_time = (0x0000FF00 & value) >> 8; // Time out
    off_time = (0x000000FF & value);     // Dark Time

    first_on_time_counter = on_time;
    second_on_time_counter = on_time;

    if (!rmtEstablished)
        ESP_GOTO_ON_ERROR(establishRMTDriver(), ind_startIndication_err, TAG, "establishRMTDriver() failed");

    // ESP_LOGW(TAG, "First  Color 0x%X Cycles 0x%X", first_color_target, first_color_cycles);
    // ESP_LOGW(TAG, "Second Color 0x%X Cycles 0x%0X", second_color_target, second_color_cycles);
    // ESP_LOGW(TAG, "On  Time 0x%02X", on_time);
    // ESP_LOGW(TAG, "Off Time 0x%02X", off_time);

    //
    // We first look for control values in the first Fist Color byte -- stored in first_color_cycles.
    // ANY VALUE OF 0, E, and F are control values.
    //
    // Value of 0 Manually sets OFF  to any LEDs designated in first_color_target.
    // Value of E Manually sets AUTO to any LEDs designated in first_color_target.
    // Value of F Manually sets ON   to any LEDs designated in first_color_target.
    //
    if (first_color_cycles == 0x0) // Cycles value is 0 -> Turn Colors Off and returns without further processing
    {
        // ESP_LOGW(TAG, "first_color_cycles = 0x0");

        if (first_color_target & COLORA_Bit)
        {
            if (aState != LED_STATE::OFF)
            {
                ESP_LOGW(TAG, "Setting  aState = LED_STATE::OFF");
                aState = LED_STATE::OFF;
                startNVSDelayTicks = xTaskGetTickCount();
            }
        }

        if (first_color_target & COLORB_Bit)
        {
            if (bState != LED_STATE::OFF)
            {
                ESP_LOGW(TAG, "Setting  bState = LED_STATE::OFF");
                bState = LED_STATE::OFF;
                startNVSDelayTicks = xTaskGetTickCount();
            }
        }

        if (first_color_target & COLORC_Bit)
        {
            if (cState != LED_STATE::OFF)
            {
                ESP_LOGW(TAG, "Setting  cState = LED_STATE::OFF");
                cState = LED_STATE::OFF;
                startNVSDelayTicks = xTaskGetTickCount();
            }
        }

        // We may need to turn LEDs off, but never turn them on here.
        clearLEDTargets = ((uint8_t)(first_color_target & COLORA_Bit) | (uint8_t)(first_color_target & COLORB_Bit) | (uint8_t)(first_color_target & COLORC_Bit));
        setAndClearColors(0, clearLEDTargets);
    }
    else if (first_color_cycles == 0xE) // Cycles value is E -> Turn Colors to AUTO State and returns without further processing
    {
        // ESP_LOGW(TAG, "first_color_cycles == 0xE");

        if (first_color_target & COLORA_Bit)
        {
            if (aState != LED_STATE::AUTO)
            {
                ESP_LOGW(TAG, "Setting  aState = LED_STATE::AUTO");
                aState = LED_STATE::AUTO;
                startNVSDelayTicks = xTaskGetTickCount();
            }
        }

        if (first_color_target & COLORB_Bit)
        {
            if (bState != LED_STATE::AUTO)
            {
                ESP_LOGW(TAG, "Setting  bState = LED_STATE::AUTO");
                bState = LED_STATE::AUTO;
                startNVSDelayTicks = xTaskGetTickCount();
            }
        }

        if (first_color_target & COLORC_Bit)
        {
            if (cState != LED_STATE::AUTO)
            {
                ESP_LOGW(TAG, "Setting  cState = LED_STATE::AUTO");
                cState = LED_STATE::AUTO;
                startNVSDelayTicks = xTaskGetTickCount();
            }
        }
        // Since all LEDs are going into AUTO mode, no colors changes are required.  Any LED is allowed to be either in an on/off state.
    }
    else if (first_color_cycles == 0xF) // Cycles value is F -> Turn Colors On and exit routine
    {
        // ESP_LOGW(TAG, "first_color_cycles = 0xF");

        if (first_color_target & COLORA_Bit)
        {
            if (aState != LED_STATE::ON)
            {
                ESP_LOGW(TAG, "Setting  aState = LED_STATE::ON");
                aState = LED_STATE::ON;
                startNVSDelayTicks = xTaskGetTickCount();
            }
        }

        if (first_color_target & COLORB_Bit)
        {
            if (bState != LED_STATE::ON)
            {
                ESP_LOGW(TAG, "Setting  bState = LED_STATE::ON");
                bState = LED_STATE::ON;
                startNVSDelayTicks = xTaskGetTickCount();
            }
        }

        if (first_color_target & COLORC_Bit)
        {
            if (cState != LED_STATE::ON)
            {
                ESP_LOGW(TAG, "Setting  cState = LED_STATE::ON");
                cState = LED_STATE::ON;
                startNVSDelayTicks = xTaskGetTickCount();
            }
        }

        // We may need to turn LEDs on, but never turn them off here.
        setLEDTargets = (uint8_t)(first_color_target & COLORA_Bit) | (uint8_t)(first_color_target & COLORB_Bit) | (uint8_t)(first_color_target & COLORC_Bit);
        setAndClearColors(setLEDTargets, 0);
    }
    else
    {
        setAndClearColors(first_color_target, 0); // Process normal color display
        first_color_cycles--;
        indState = IND_STATES::Clear_FirstColor;
        IsIndicating = true;
    }
    return;

ind_startIndication_err:
    errMsg = std::string(__func__) + "(): " + esp_err_to_name(ret);
    indOP = IND_OP::Error;
}

void Indication::setAndClearColors(uint8_t SetColors, uint8_t ClearColors)
{
    esp_err_t ret = ESP_OK;
    uint8_t led_strip_pixels[3];

    led_strip_pixels[0] = bCurrValue; // Green
    led_strip_pixels[1] = aCurrValue; // Red
    led_strip_pixels[2] = cCurrValue; // Blue

    if (ClearColors & COLORA_Bit) // Seeing the bit to Clear this color
    {
        if (aState == LED_STATE::ON)
            aCurrValue = aSetLevel; // Don't turn off this value because our state is ON
        else
            aCurrValue = 0; // Otherwise, turn it off.

        led_strip_pixels[1] = aCurrValue; // Red
        // ESP_LOGW(TAG, "Red    State = %s / Value = %d", getStateText(aState).c_str(), aCurrValue);
    }

    if (ClearColors & COLORB_Bit)
    {
        if (bState == LED_STATE::ON)
            bCurrValue = bSetLevel;
        else
            bCurrValue = 0;

        led_strip_pixels[0] = bCurrValue; // Green
        // ESP_LOGW(TAG, "Green  State = %s / Value = %d", getStateText(bState).c_str(), bCurrValue);
    }

    if (ClearColors & COLORC_Bit)
    {
        if (cState == LED_STATE::ON)
            cCurrValue = cSetLevel;
        else
            cCurrValue = 0;

        led_strip_pixels[2] = cCurrValue; // Blue
        // ESP_LOGW(TAG, "Blue   State = %s / Value = %d", getStateText(cState).c_str(), cCurrValue);
    }

    if (SetColors & COLORA_Bit) // Setting the bit to Set this color
    {
        if (aState == LED_STATE::OFF) // Set the color if the state is set above OFF
            aCurrValue = 0;           // Don't allow any value to be displayed on the LED
        else
            aCurrValue = aSetLevel; // State is either AUTO or ON.

        led_strip_pixels[1] = aCurrValue; // Red
        // ESP_LOGW(TAG, "Red    State = %s / Value = %d", getStateText(aState).c_str(), aCurrValue);
    }

    if (SetColors & COLORB_Bit)
    {
        if (bState == LED_STATE::OFF)
            bCurrValue = 0;
        else
            bCurrValue = bSetLevel;

        led_strip_pixels[0] = bCurrValue; // Green
        // ESP_LOGW(TAG, "Green  State = %s / Value = %d", getStateText(bState).c_str(), bCurrValue);
    }

    if (SetColors & COLORC_Bit)
    {
        if (cState == LED_STATE::OFF)
            cCurrValue = 0;
        else
            cCurrValue = cSetLevel;

        led_strip_pixels[2] = cCurrValue; // Blue
        // ESP_LOGW(TAG, "Blue   State = %s / Value = %d", getStateText(cState).c_str(), cCurrValue);
    }

    ESP_GOTO_ON_ERROR(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config), ind_setAndClearColors_err, TAG, "rmt_transmit() failed");
    ESP_GOTO_ON_ERROR(rmt_tx_wait_all_done(led_chan, portMAX_DELAY), ind_setAndClearColors_err, TAG, "rmt_tx_wait_all_done() failed");
    return;

ind_setAndClearColors_err:
    errMsg = std::string(__func__) + "(): " + esp_err_to_name(ret);
    indOP = IND_OP::Error;
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

    indState = IND_STATES::Idle;
    IsIndicating = false;
}