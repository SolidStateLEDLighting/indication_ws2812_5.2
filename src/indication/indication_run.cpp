#include "indication/indication_.hpp"

extern SemaphoreHandle_t semIndEntry;

void Indication::runMarshaller(void *arg)
{
    ((Indication *)arg)->run();
    vTaskDelete(NULL);
}

void Indication::run(void)
{
    esp_err_t ret = ESP_OK;

    uint8_t cycles = 0;
    uint32_t value = 0;
    //
    // Set Object Debug Level
    // Note that this function can not raise log level above the level set using CONFIG_LOG_DEFAULT_LEVEL setting in menuconfig.
    // esp_log_level_set(TAG, ESP_LOG_INFO);
    //
    TickType_t startTime;
    TickType_t waitTime = 100;

    while (true)
    {
        switch (indOP)
        {
        case IND_OP::Run:
        {
            if (IsIndicating)
            {
                waitTime = 1;
                startTime = xTaskGetTickCount();
                vTaskDelayUntil(&startTime, waitTime);

                switch (indStates)
                {
                case IND_STATES::Init:
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
                            { // If we are finished with the first color and we don't have a second color add extra delay time.
                                if ((first_color_cycles < 1) && (second_color_target == 0))
                                    off_time_counter = 3 * off_time;
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
            else // When we are not indicating -- we are looking for notifications or incoming commands
            {
                indTaskNotifyValue = static_cast<IND_NOTIFY>(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5)));

                if (indTaskNotifyValue > IND_NOTIFY::NONE)
                {
                    uint8_t newValue = (int)indTaskNotifyValue & 0x000000FF;

                    if (show & _showRun)
                        ESP_LOGW(TAG, "Task notification Colors 0x%02X  newValue is %d", ((((int)indTaskNotifyValue) & 0xFFFFFF00) >> 8), newValue);

                    if (newValue > 0) // All notifications here be applied to all colors may be set at the same time.
                    {
                        if ((int)indTaskNotifyValue & (int)IND_NOTIFY::SET_A_COLOR_BRIGHTNESS)
                            aSetLevel = newValue;

                        if ((int)indTaskNotifyValue & (int)IND_NOTIFY::SET_B_COLOR_BRIGHTNESS)
                            bSetLevel = newValue;

                        if ((int)indTaskNotifyValue & (int)IND_NOTIFY::SET_C_COLOR_BRIGHTNESS)
                            cSetLevel = newValue;

                        saveToNVSDelayCount = 5;
                    }
                }
            }

            if (xQueueReceive(queHandleIndCmdRequest, (void *)&value, pdMS_TO_TICKS(195))) // We can wait here most of the time for requests
                startIndication(value);

            if (saveToNVSDelayCount > 0)
            {
                // ESP_LOGW(TAG, "%d", saveToNVSDelayCount);
                if (--saveToNVSDelayCount < 1)
                {
                    if (!saveVariablesToNVS())
                        ESP_LOGE(TAG, "saveVariablesToNVS() failed.");
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
                initIndStep = IND_INIT::Early_Release;
                break;
            }

            case IND_INIT::Early_Release:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::Early_Release - Step " + std::to_string((int)IND_INIT::Early_Release));
                //
                // In the event that no one else is using the LED for any kind of indication during initialization, the object can release it's locking
                // semaphore early.
                //
                xSemaphoreGive(semIndEntry); // Releasing early because no one else uses indication during startup
                initIndStep = IND_INIT::Set_Variables_From_Config;
                break;
            }

            case IND_INIT::Set_Variables_From_Config:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::Set_Variables_From_Config - Step " + std::to_string((int)IND_INIT::Set_Variables_From_Config));

                // We just restored all the Color State...
                // Now we need to act on them to put the LEDs any restrictive states as needed...

                if (aState == LED_STATE::ON)
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

                ESP_GOTO_ON_ERROR(rmt_new_tx_channel(&tx_chan_config, &led_chan), CreateRMTTxChannel_err, TAG, "rmt_new_tx_channel() failed");
                initIndStep = IND_INIT::CreateRMTEncoder;
                break;

            CreateRMTTxChannel_err:
                ESP_LOGE(TAG, "error: %s", esp_err_to_name(ret));
                initIndStep = IND_INIT::Finished;
                break;
            }

            case IND_INIT::CreateRMTEncoder:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::CreateRMTEncoder - Step " + std::to_string((int)IND_INIT::CreateRMTEncoder));

                led_strip_encoder_config_t encoder_config = {
                    RMT_LED_STRIP_RESOLUTION_HZ,
                };

                ret = rmt_new_led_strip_encoder(&encoder_config, &led_encoder);

                if unlikely (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "%s::rmt_new_led_strip_encoder() failed.  err = %d - %s", __func__, ret, esp_err_to_name(ret));
                    initIndStep = IND_INIT::Finished;
                    break;
                }

                initIndStep = IND_INIT::EnableRMTChannel;
                break;
            }

            case IND_INIT::EnableRMTChannel:
            {
                if (show & _showInit)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): IND_INIT::EnableRMTChannel - Step " + std::to_string((int)IND_INIT::EnableRMTChannel));

                ret = rmt_enable(led_chan);

                if unlikely (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "%s::rmt_enable() failed.  err = %d - %s", __func__, ret, esp_err_to_name(ret));
                    initIndStep = IND_INIT::Finished;
                    break;
                }

                static uint8_t led_strip_pixels[3]; // Clear the display
                memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
                ESP_GOTO_ON_ERROR(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config), EnableRMTChannel_err, TAG, "rmt_transmit() failed");
                ESP_GOTO_ON_ERROR(rmt_tx_wait_all_done(led_chan, portMAX_DELAY), EnableRMTChannel_err, TAG, "rmt_tx_wait_all_done() failed");

                cycles = majorVer;
                initIndStep = IND_INIT::ColorA_On;
                break;

            EnableRMTChannel_err:
                ESP_LOGE(TAG, "Error is %s", esp_err_to_name(ret));
                initIndStep = IND_INIT::Finished;
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
                    cycles = revNumber;
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
        }
        taskYIELD();
    }
}

esp_err_t Indication::startIndication(uint32_t value)
{
    esp_err_t ret = ESP_OK;
    //  std::cout << "IO Value: " << (uint32_t *)value << std::endl;
    first_color_target = (0xF0000000 & value) >> 28; // First Color(s) -- We may see any of the color bits set
    first_color_cycles = (0x0F000000 & value) >> 24; // First Color Cycles

    second_color_target = (0x00F00000 & value) >> 20; // Second Color(s)
    second_color_cycles = (0x000F0000 & value) >> 16; // Second Color Cycles

    on_time = (0x0000FF00 & value) >> 8; // Time out
    off_time = (0x000000FF & value);     // Dark Time

    first_on_time_counter = on_time;
    second_on_time_counter = on_time;

    ESP_LOGW(TAG, "First Color 0x%X Cycles 0x%X", first_color_target, first_color_cycles);
    ESP_LOGW(TAG, "Second Color 0x%X Cycles 0x%0X", second_color_target, second_color_cycles);
    ESP_LOGW(TAG, "Color Time 0x%02X", on_time);
    ESP_LOGW(TAG, "Dark Time 0x%02X", off_time);

    //
    // Manually turning ON and OFF or AUTO -- the led LEDs happens always in the First Color byte
    //
    if (first_color_cycles == 0x0) // Cycles value is 0 -> Turn Colors Off and exit routine
    {
        ESP_LOGW(TAG, "first_color_cycles = 0x0");

        if (first_color_target & COLORA_Bit)
            aState = LED_STATE::ON;

        if (first_color_target & COLORB_Bit)
            bState = LED_STATE::ON;

        if (first_color_target & COLORC_Bit)
            cState = LED_STATE::ON;

        setLEDTargets = 0;
        clearLEDTargets = (uint8_t)(first_color_target & COLORA_Bit) | (uint8_t)(first_color_target & COLORB_Bit) | (uint8_t)(first_color_target & COLORC_Bit);
        setAndClearColors(setLEDTargets, clearLEDTargets);
    }
    else if (first_color_cycles == 0xE) // Cycles value is E -> Turn Colors to AUTO State and exit routine
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
    else if (first_color_cycles == 0xF) // Cycles value is F -> Turn Colors On and exit routine
    {
        ESP_LOGW(TAG, "first_color_cycles = 0xF");

        if (first_color_target & COLORA_Bit)
            aState = LED_STATE::OFF;

        if (first_color_target & COLORB_Bit)
            bState = LED_STATE::OFF;

        if (first_color_target & COLORC_Bit)
            cState = LED_STATE::OFF;

        clearLEDTargets = 0;
        setLEDTargets = (uint8_t)(first_color_target & COLORA_Bit) | (uint8_t)(first_color_target & COLORB_Bit) | (uint8_t)(first_color_target & COLORC_Bit);
        setAndClearColors(setLEDTargets, clearLEDTargets);
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
    // ESP_LOGI(TAG, "Colors Curr/Default  %d/%d   %d/%d   %d/%d", aCurrValue, aDefaultLevel, bCurrValue, bDefaultLevel, cCurrValue, cDefaultValue);
    //
    if (ClearColors & COLORA_Bit) // Seeing the bit to Clear this color
    {
        if (aState == LED_STATE::ON)
            aCurrValue = aSetLevel; // Don't turn off this value because our state is ON
        else
            aCurrValue = 0; // Otherwise, turn it off.

        led_strip_pixels[1] = aCurrValue; // Red
        ESP_LOGW(TAG, "Red    State = %s / Value = %d", getStateText(aState).c_str(), aCurrValue);
    }

    if (ClearColors & COLORB_Bit)
    {
        if (bState == LED_STATE::ON)
            bCurrValue = bSetLevel;
        else
            bCurrValue = 0;

        led_strip_pixels[0] = bCurrValue; // Green
        ESP_LOGW(TAG, "Green  State = %s / Value = %d", getStateText(bState).c_str(), bCurrValue);
    }

    if (ClearColors & COLORC_Bit)
    {
        if (cState == LED_STATE::ON)
            cCurrValue = cSetLevel;
        else
            cCurrValue = 0;

        led_strip_pixels[2] = cCurrValue; // Blue
        ESP_LOGW(TAG, "Blue   State = %s / Value = %d", getStateText(cState).c_str(), cCurrValue);
    }

    if (SetColors & COLORA_Bit) // Setting the bit to Set this color
    {
        if (aState == LED_STATE::OFF) // Set the color if the state is set above OFF
            aCurrValue = 0;           // Don't allow any value to be displayed on the LED
        else
            aCurrValue = aSetLevel; // State is either AUTO or ON.

        led_strip_pixels[1] = aCurrValue; // Red
        ESP_LOGW(TAG, "Red    State = %s / Value = %d", getStateText(aState).c_str(), aCurrValue);
    }

    if (SetColors & COLORB_Bit)
    {
        if (bState == LED_STATE::OFF)
            bCurrValue = 0;
        else
            bCurrValue = bSetLevel;

        led_strip_pixels[0] = bCurrValue; // Green
        ESP_LOGW(TAG, "Green  State = %s / Value = %d", getStateText(bState).c_str(), bCurrValue);
    }

    if (SetColors & COLORC_Bit)
    {
        if (cState == LED_STATE::OFF)
            cCurrValue = 0;
        else
            cCurrValue = cSetLevel;

        led_strip_pixels[2] = cCurrValue; // Blue
        ESP_LOGW(TAG, "Blue   State = %s / Value = %d", getStateText(cState).c_str(), cCurrValue);
    }

    /* ESP_LOGW(TAG, "---------------------------------------------------");
    ESP_LOGW(TAG, "Red    State = %s / Value = %d", getStateText(aState).c_str(), aCurrValue);
    ESP_LOGW(TAG, "Green  State = %s / Value = %d", getStateText(bState).c_str(), bCurrValue);
    ESP_LOGW(TAG, "Blue   State = %s / Value = %d", getStateText(cState).c_str(), cCurrValue);
    ESP_LOGW(TAG, "---------------------------------------------------"); */

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