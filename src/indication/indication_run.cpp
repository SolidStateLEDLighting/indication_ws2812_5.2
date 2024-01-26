#include "indication/indication_.hpp"

#include "driver/rmt_tx.h"

extern SemaphoreHandle_t semIndEntry;

void Indication::runMarshaller(void *arg)
{
    ((Indication *)arg)->run();
    ((Indication *)arg)->taskHandleRun = nullptr;
    vTaskDelete(((Indication *)arg)->taskHandleRun);
}

void Indication::run(void)
{
    esp_err_t ret = ESP_OK;

    uint8_t cycles = 0;
    uint32_t value = 0;

    TickType_t startTime;
    TickType_t waitTime = 100;

    resetIndication();

    while (true)
    {
        switch (indOP)
        {
        case IND_OP::Run: // We would like to achieve about a 5Hz entry cadence in the Run state - WHEN NOT INDICATING
        {
            if (IsIndicating)
            {
                waitTime = 1;
                startTime = xTaskGetTickCount();
                vTaskDelayUntil(&startTime, waitTime);

                switch (indStates)
                {
                case IND_STATES::Idle:
                {
                    break;
                }

                case IND_STATES::Show_FirstColor:
                {
                    if (first_on_time_counter > 0)
                    {
                        if (--first_on_time_counter < 1)
                        {
                            setAndClearColors(0, first_color_target);
                            indStates = IND_STATES::FirstColor_Dark;

                            if (first_color_cycles == 0)
                            {
                                off_time_counter = (off_time * 2);
                                indStates = IND_STATES::SecondColor_Dark;
                            }
                            else
                            {
                                if ((first_color_cycles < 1) && (second_color_target == 0))
                                    off_time_counter = 3 * off_time; // If we are finished with the first color and we don't have a second color add extra delay time.
                                else
                                    off_time_counter = off_time;
                            }
                        }
                    }
                    break;
                }

                case IND_STATES::Show_SecondColor:
                {
                    if (second_on_time_counter > 0)
                    {
                        if (--second_on_time_counter < 1)
                        {
                            setAndClearColors(0, second_color_target);
                            indStates = IND_STATES::SecondColor_Dark;

                            if (second_color_cycles < 1) // If we are finished with the second color -- add extra delay time
                                off_time_counter = 3 * off_time;
                            else
                                off_time_counter = off_time;
                        }
                    }
                    break;
                }

                case IND_STATES::FirstColor_Dark:
                case IND_STATES::SecondColor_Dark:
                {
                    if (off_time_counter > 0)
                    {
                        if (--off_time_counter < 1)
                        {
                            if (first_color_cycles > 0)
                            {
                                first_color_cycles--;
                                setAndClearColors(first_color_target, 0); // Turn on the LED

                                first_on_time_counter = on_time;
                                indStates = IND_STATES::Show_FirstColor;
                            }
                            else if ((second_color_cycles > 0) && (indStates == IND_STATES::SecondColor_Dark))
                            {
                                second_color_cycles--;
                                setAndClearColors(second_color_target, 0); // Turn on the LED

                                second_on_time_counter = on_time;
                                indStates = IND_STATES::Show_SecondColor;
                            }
                            else
                            {
                                indStates = IND_STATES::Final;
                            }
                        }
                    }

                    break;
                }

                case IND_STATES::Final:
                {
                    resetIndication(); // Resetting all the indicator variables
                    break;
                }
                }

                break;
            }
            else // When we are not indicating -- we are looking for notifications or incoming commands.
            {
                indTaskNotifyValue = static_cast<IND_NOTIFY>(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5)));

                if (indTaskNotifyValue > IND_NOTIFY::NONE)
                {
                    // ESP_LOGW(TAG, "Task notification Colors 0x%02X  Value is %d", ((((int)indTaskNotifyValue) & 0xFFFFFF00) >> 8), (int)indTaskNotifyValue & 0x000000FF);

                    if ((int)indTaskNotifyValue & (int)IND_NOTIFY::SET_A_COLOR_BRIGHTNESS)
                    {
                        aSetLevel = (int)indTaskNotifyValue & 0x000000FF;
                        saveToNVSDelayCount = 8;
                    }
                    else if ((int)indTaskNotifyValue & (int)IND_NOTIFY::SET_B_COLOR_BRIGHTNESS)
                    {
                        bSetLevel = (int)indTaskNotifyValue & 0x000000FF;
                        saveToNVSDelayCount = 8;
                    }

                    else if ((int)indTaskNotifyValue & (int)IND_NOTIFY::SET_C_COLOR_BRIGHTNESS)
                    {
                        cSetLevel = (int)indTaskNotifyValue & 0x000000FF;
                        saveToNVSDelayCount = 8;
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
            }

            if (xQueueReceive(queHandleIndCmdRequest, (void *)&value, pdMS_TO_TICKS(195)) == pdTRUE) // We can wait here most of the time for requests
            {
                // ESP_LOGW(TAG, "Received notification value of %08X", (int)value);
                startIndication(value); // We have an indication value
            }

            if (saveToNVSDelayCount > 0) // Counts of 4 equal about 1 second.
            {
                if (--saveToNVSDelayCount < 1)
                    saveVariablesToNVS();
            }
            break;
        }

            // Positionally, it is important for Shutdown to be serviced right after it is called.  We don't want other possible operations
            // becoming active unexepectedly.  This is somewhat important.
        case IND_OP::Shutdown:
        {
            // A shutdown is a complete undoing of all items that were established or created with our run thread.
            // If we are connected then disconnect.  If we created resources, we must dispose of them here.
            switch (indShdnStep)
            {
            case IND_SHUTDOWN::Start:
            {
                if (showIND & _showINDShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_SHUTDOWN::Start");

                indShdnStep = IND_SHUTDOWN::DisableAndDeleteRMTChannel;
                break;
            }

            case IND_SHUTDOWN::DisableAndDeleteRMTChannel:
            {
                if (showIND & _showINDShdnSteps)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_SHUTDOWN::DisableAndDeleteRMTChannel - Step " + std::to_string((int)IND_SHUTDOWN::DisableAndDeleteRMTChannel));

                ESP_GOTO_ON_ERROR(rmt_disable(led_chan), ind_disableAndDeleteRMTChannel_err, TAG, "rmt_disable() failed");
                ESP_GOTO_ON_ERROR(rmt_del_encoder(led_encoder), ind_disableAndDeleteRMTChannel_err, TAG, "rmt_del_encoder() failed");
                ESP_GOTO_ON_ERROR(rmt_del_channel(led_chan), ind_disableAndDeleteRMTChannel_err, TAG, "rmt_del_channel() failed");
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

                /* Delete Command Queue */
                if (queHandleIndCmdRequest != nullptr)
                {
                    vQueueDelete(queHandleIndCmdRequest);
                    queHandleIndCmdRequest = nullptr;
                }

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
            switch (initIndStep)
            {
            case IND_INIT::Start:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::Start");

                initIndStep = IND_INIT::Init_Queues_Commands;
                break;
            }

            case IND_INIT::Init_Queues_Commands:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::Init_Queues_Commands - Step " + std::to_string((int)IND_INIT::Init_Queues_Commands));

                queHandleIndCmdRequest = xQueueCreate(3, sizeof(uint32_t)); // We can receive up to 3 indication requests and they will play out in order.
                initIndStep = IND_INIT::CreateRMTTxChannel;
                break;
            }

            case IND_INIT::CreateRMTTxChannel:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::CreateRMTTxChannel - Step " + std::to_string((int)IND_INIT::CreateRMTTxChannel));

                // ESP_LOGW(TAG, "RMT_LED_STRIP_GPIO_NUM %d", RMT_LED_STRIP_GPIO_NUM);
                // ESP_LOGW(TAG, "RMT_CLK_SRC_DEFAULT %d", RMT_CLK_SRC_DEFAULT);
                // ESP_LOGW(TAG, "RMT_LED_STRIP_RESOLUTION_HZ %d", RMT_LED_STRIP_RESOLUTION_HZ);

                rmt_tx_channel_config_t tx_chan_config = {
                    (gpio_num_t)RMT_LED_STRIP_GPIO_NUM, // selects GPIO
                    RMT_CLK_SRC_DEFAULT,                // selects source clock
                    RMT_LED_STRIP_RESOLUTION_HZ,        //
                    64,                                 // Increasing the block size can make the LED less flickering
                    4,                                  // Set the number of transactions that can be pending in the background
                    0,                                  // Interrupt Priority
                    {0, 1, 1, 0},                       // invert_out, with_dma, io_loop_back, io_od_mode
                };

                ESP_GOTO_ON_ERROR(rmt_new_tx_channel(&tx_chan_config, &led_chan), ind_createRMTTxChannel_err, TAG, "rmt_new_tx_channel() failed");
                initIndStep = IND_INIT::CreateRMTEncoder;
                break;

            ind_createRMTTxChannel_err:
                errMsg = std::string(__func__) + "(): " + esp_err_to_name(ret);
                indOP = IND_OP::Error;
                break;
            }

            case IND_INIT::CreateRMTEncoder:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::CreateRMTEncoder - Step " + std::to_string((int)IND_INIT::CreateRMTEncoder));

                led_strip_encoder_config_t encoder_config = {
                    RMT_LED_STRIP_RESOLUTION_HZ,
                };

                ESP_GOTO_ON_ERROR(rmt_new_led_strip_encoder(&encoder_config, &led_encoder), ind_createRMTEncoder_err, TAG, "rmt_new_led_strip_encoder() failed");

                initIndStep = IND_INIT::EnableRMTChannel;
                break;

            ind_createRMTEncoder_err:
                errMsg = std::string(__func__) + "(): " + esp_err_to_name(ret);
                indOP = IND_OP::Error;
                break;
            }

            case IND_INIT::EnableRMTChannel:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::EnableRMTChannel - Step " + std::to_string((int)IND_INIT::EnableRMTChannel));

                ESP_GOTO_ON_ERROR(rmt_enable(led_chan), ind_enableRMTChannel_err, TAG, "rmt_enable() failed");

                static uint8_t led_strip_pixels[3]; // Clear the display
                memset(led_strip_pixels, 0, sizeof(led_strip_pixels));

                ESP_GOTO_ON_ERROR(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config), ind_enableRMTChannel_err, TAG, "rmt_transmit() failed");
                ESP_GOTO_ON_ERROR(rmt_tx_wait_all_done(led_chan, portMAX_DELAY), ind_enableRMTChannel_err, TAG, "rmt_tx_wait_all_done() failed");

                cycles = majorVer;
                initIndStep = IND_INIT::Set_LED_Initial_States;
                break;

            ind_enableRMTChannel_err:
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
                else if (aState == LED_STATE::OFF)
                    setAndClearColors(0, COLORA_Bit);

                if (bState == LED_STATE::ON)
                    setAndClearColors(COLORB_Bit, 0);
                else if (aState == LED_STATE::OFF)
                    setAndClearColors(0, COLORB_Bit);

                if (cState == LED_STATE::ON)
                    setAndClearColors(COLORC_Bit, 0);
                else if (cState == LED_STATE::OFF)
                    setAndClearColors(0, COLORC_Bit);

                initIndStep = IND_INIT::Early_Release;
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
                    initIndStep = IND_INIT::Finished;
                else if ((aState == LED_STATE::OFF) && (bState == LED_STATE::OFF) && (cState == LED_STATE::OFF))
                    initIndStep = IND_INIT::Finished;
                else
                    initIndStep = IND_INIT::ColorA_On;
                break;
            }

            case IND_INIT::ColorA_On:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorA_On - Step " + std::to_string((int)IND_INIT::ColorA_On));

                setAndClearColors((uint8_t)COLORA_Bit, 0);

                vTaskDelay(pdMS_TO_TICKS(100));
                initIndStep = IND_INIT::ColorA_Off;
                break;
            }

            case IND_INIT::ColorA_Off:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorA_Off - Step " + std::to_string((int)IND_INIT::ColorA_Off));

                setAndClearColors(0, (uint8_t)COLORA_Bit);

                vTaskDelay(pdMS_TO_TICKS(150));

                if (cycles > 0)
                    cycles--;

                if (cycles > 0)
                {
                    initIndStep = IND_INIT::ColorA_On;
                }
                else
                {
                    cycles = minorVer;
                    initIndStep = IND_INIT::ColorB_On;
                    vTaskDelay(pdMS_TO_TICKS(250));
                }
                break;
            }

            case IND_INIT::ColorB_On:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorB_On - Step " + std::to_string((int)IND_INIT::ColorB_On));

                setAndClearColors((uint8_t)COLORB_Bit, 0);

                vTaskDelay(pdMS_TO_TICKS(100));
                initIndStep = IND_INIT::ColorB_Off;
                break;
            }

            case IND_INIT::ColorB_Off:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorB_Off - Step " + std::to_string((int)IND_INIT::ColorB_Off));

                setAndClearColors(0, (uint8_t)COLORB_Bit);

                vTaskDelay(pdMS_TO_TICKS(150));

                if (cycles > 0)
                    cycles--;

                if (cycles > 0)
                {
                    initIndStep = IND_INIT::ColorB_On;
                }
                else
                {
                    cycles = patchNumber;
                    initIndStep = IND_INIT::ColorC_On;
                    vTaskDelay(pdMS_TO_TICKS(250));
                }
                break;
            }

            case IND_INIT::ColorC_On:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorC_On - Step " + std::to_string((int)IND_INIT::ColorC_On));

                setAndClearColors((uint8_t)COLORC_Bit, 0);

                vTaskDelay(pdMS_TO_TICKS(100));
                initIndStep = IND_INIT::ColorC_Off;
                break;
            }

            case IND_INIT::ColorC_Off:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::ColorC_Off - Step " + std::to_string((int)IND_INIT::ColorC_Off));

                setAndClearColors(0, (uint8_t)COLORC_Bit);

                vTaskDelay(pdMS_TO_TICKS(150));

                if (cycles > 0)
                    cycles--;

                if (cycles > 0)
                {
                    initIndStep = IND_INIT::ColorC_On;
                }
                else
                {
                    initIndStep = IND_INIT::Finished;
                    vTaskDelay(pdMS_TO_TICKS(250));
                }
                break;
            }

            case IND_INIT::Finished:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::Finished");

                indOP = IND_OP::Run;
                // xSemaphoreGive(semIndEntry); // Yield now if not done earlier inside Early_Release
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
            indOP = IND_OP::Idle_Silent;
            [[fallthrough]];
        }

        case IND_OP::Idle_Silent:
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            break;
        }
        }
        taskYIELD();
    }
}

void Indication::startIndication(uint32_t value)
{
    bool _saveStatesToNVS = false;

    first_color_target = (0xF0000000 & value) >> 28; // First Color(s) -- We may see any of the color bits set
    first_color_cycles = (0x0F000000 & value) >> 24; // First Color Cycles

    second_color_target = (0x00F00000 & value) >> 20; // Second Color(s)
    second_color_cycles = (0x000F0000 & value) >> 16; // Second Color Cycles

    on_time = (0x0000FF00 & value) >> 8; // Time out
    off_time = (0x000000FF & value);     // Dark Time

    first_on_time_counter = on_time;
    second_on_time_counter = on_time;

    // ESP_LOGW(TAG, "First Color 0x%X Cycles 0x%X", first_color_target, first_color_cycles);
    // ESP_LOGW(TAG, "Second Color 0x%X Cycles 0x%0X", second_color_target, second_color_cycles);
    // ESP_LOGW(TAG, "Color Time 0x%02X", on_time);
    // ESP_LOGW(TAG, "Dark Time 0x%02X", off_time);

    //
    // Manually turning ON and OFF or AUTO -- the led LEDs happens always in the First Color byte
    //
    if (first_color_cycles == 0x0) // Cycles value is 0 -> Turn Colors Off and exit routine
    {
        // ESP_LOGW(TAG, "first_color_cycles = 0x0");

        if (first_color_target & COLORA_Bit)
        {
            if (aState != LED_STATE::OFF)
            {
                aState = LED_STATE::OFF;
                _saveStatesToNVS = true;
            }
        }

        if (first_color_target & COLORB_Bit)
        {
            if (bState != LED_STATE::OFF)
            {
                bState = LED_STATE::OFF;
                _saveStatesToNVS = true;
            }
        }

        if (first_color_target & COLORC_Bit)
        {
            if (cState != LED_STATE::OFF)
            {
                cState = LED_STATE::OFF;
                _saveStatesToNVS = true;
            }
        }

        // We may need to turn LEDs off, but never turn them on here.
        clearLEDTargets = (uint8_t)(first_color_target & COLORA_Bit) | (uint8_t)(first_color_target & COLORB_Bit) | (uint8_t)(first_color_target & COLORC_Bit);
        setAndClearColors(0, clearLEDTargets);
    }
    else if (first_color_cycles == 0xE) // Cycles value is E -> Turn Colors to AUTO State and exit routine
    {
        // ESP_LOGW(TAG, "first_color_cycles == 0xE");

        if (first_color_target & COLORA_Bit)
        {
            if (aState != LED_STATE::AUTO)
            {
                aState = LED_STATE::AUTO;
                _saveStatesToNVS = true;
            }
        }

        if (first_color_target & COLORB_Bit)
        {
            if (bState != LED_STATE::AUTO)
            {
                bState = LED_STATE::AUTO;
                _saveStatesToNVS = true;
            }
        }

        if (first_color_target & COLORC_Bit)
        {
            if (cState != LED_STATE::AUTO)
            {
                cState = LED_STATE::AUTO;
                _saveStatesToNVS = true;
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
                aState = LED_STATE::ON;
                _saveStatesToNVS = true;
            }
        }

        if (first_color_target & COLORB_Bit)
        {
            if (bState != LED_STATE::ON)
            {
                bState = LED_STATE::ON;
                _saveStatesToNVS = true;
            }
        }

        if (first_color_target & COLORC_Bit)
        {
            if (cState != LED_STATE::ON)
            {
                cState = LED_STATE::ON;
                _saveStatesToNVS = true;
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
        indStates = IND_STATES::Show_FirstColor;
        IsIndicating = true;
    }

    if (_saveStatesToNVS)
        saveToNVSDelayCount = 8;
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

    indStates = IND_STATES::Idle;
    IsIndicating = false;
}