#include "indication/indication_.hpp"
#include "system_.hpp" // Class structure and variables

#include "esp_check.h"

/* External Variables */
extern SemaphoreHandle_t semNVSEntry;

/* NVS Routines */
bool Indication::restoreVariablesFromNVS(void)
{
    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "()");

    uint8_t temp = 0;

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY) == pdTRUE)
    {
        if (nvs == nullptr)
            nvs = NVS::getInstance(); // First, get the nvs object handle if didn't already.

        if unlikely (nvs->openNVSStorage("indication", true) == false)
        {
            ESP_LOGE(TAG, "Error, Unable to OpenNVStorage inside restoreVariablesFromNVS");
            xSemaphoreGive(semNVSEntry);
            return false;
        }
    }

    bool successFlag = true;

    if (successFlag) // Restore runStackSizeK
    {
        temp = runStackSizeK; // This will save the default size if value doesn't exist yet in nvs.

        if (nvs->getU8IntegerFromNVS("runStackSizeK", &temp))
        {
            if (temp > runStackSizeK) // Ok to use any value greater than the default size.
            {
                runStackSizeK = temp;
                saveToNVSDelayCount = 8; // Save it
            }

            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): runStackSizeK is " + std::to_string(runStackSizeK));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore runStackSizeK");
        }
    }

    /* Restore Color States */
    std::string key = "aState";
    uint8_t int8Val = 0;

    if unlikely (nvs->getU8IntegerFromNVS(key.c_str(), &int8Val))
    {
        if (int8Val == 0) // If the value has never been saved, set it to AUTO and mark it dirty
        {
            int8Val = (uint8_t)aState;
            aState_nvs_dirty = true;
            saveToNVSDelayCount = 10;
        }

        if (show & _showNVS)
            ESP_LOGI(TAG, "aState is %s", getStateText((int)aState).c_str());
    }
    else
    {
        ESP_LOGE(TAG, "getU8IntegerFromNVS failed on key %s", key.c_str());
        nvs->closeNVStorage(false); // No changes
        xSemaphoreGive(semNVSEntry);
        return false;
    }

    key = "bState";

    if unlikely (nvs->getU8IntegerFromNVS(key.c_str(), &int8Val))
    {
        if (int8Val == 0)
        {
            int8Val = (uint8_t)bState;
            bState_nvs_dirty = true;
            saveToNVSDelayCount = 10;
        }

        if (show & _showNVS)
            ESP_LOGI(TAG, "bState is %s", getStateText((int)bState).c_str());
    }
    else
    {
        ESP_LOGE(TAG, "getU8IntegerFromNVS failed on key %s", key.c_str());
        nvs->closeNVStorage(false); // No changes
        xSemaphoreGive(semNVSEntry);
        return false;
    }

    key = "cState";

    if unlikely (nvs->getU8IntegerFromNVS(key.c_str(), &int8Val))
    {
        if (int8Val == 0)
        {
            int8Val = (uint8_t)cState;
            cState_nvs_dirty = true;
            saveToNVSDelayCount = 10;
        }

        if (show & _showNVS)
            ESP_LOGI(TAG, "cState is %s", getStateText((int)cState).c_str());
    }
    else
    {
        ESP_LOGE(TAG, "getU8IntegerFromNVS failed on key %s", key.c_str());
        nvs->closeNVStorage(false); // No changes
        xSemaphoreGive(semNVSEntry);
        return false;
    }

    /* Restore Color Values */
    key = "aDefValue"; // Color A Default Value

    if unlikely (nvs->getU8IntegerFromNVS(key.c_str(), &int8Val))
    {
        if (int8Val > 0)             // Do not allow the Color Value to be set lower than 1
            aDefaultValue = int8Val; // The key idea is that we are restoring a saved default value.
        else                         // Value is zero which is unacceptable.
        {
            aDefaultValue = 1;
            aDefaultValue_nvs_dirty = true;
            saveToNVSDelayCount = 10;
        }

        if (show & _showNVS) // If we don't have something stored, then we still have the program's default value.
            ESP_LOGI(TAG, "Color A Value Default %d", aDefaultValue);
    }
    else
    {
        ESP_LOGE(TAG, "getU8IntegerFromNVS failed on key %s", key.c_str());
        nvs->closeNVStorage(false); // No changes
        xSemaphoreGive(semNVSEntry);
        return false;
    }

    key = "bDefValue";

    if unlikely (nvs->getU8IntegerFromNVS(key.c_str(), &int8Val))
    {
        if (int8Val > 0)
            bDefaultValue = int8Val;
        else
        {
            bDefaultValue = 1;
            bDefaultValue_nvs_dirty = true;
            saveToNVSDelayCount = 10;
        }

        if (show & _showNVS)
            ESP_LOGI(TAG, "Color B Value Default %d", bDefaultValue);
    }
    else
    {
        ESP_LOGE(TAG, "getU8IntegerFromNVS failed on key %s", key.c_str());
        nvs->closeNVStorage(false); // No changes
        xSemaphoreGive(semNVSEntry);
        return false;
    }

    key = "cDefValue";

    if unlikely (nvs->getU8IntegerFromNVS(key.c_str(), &int8Val))
    {
        if (int8Val > 0)
            cDefaultValue = int8Val;
        else
        {
            cDefaultValue = 1;
            cDefaultValue_nvs_dirty = true;
            saveToNVSDelayCount = 10;
        }

        if (show & _showNVS)
            ESP_LOGI(TAG, "Color C Value Default %d", cDefaultValue);
    }
    else
    {
        ESP_LOGE(TAG, "getU8IntegerFromNVS failed on key %s", key.c_str());
        nvs->closeNVStorage(false); // No changes
        xSemaphoreGive(semNVSEntry);
        return false;
    }

    nvs->closeNVStorage(false); // No commit to any changes
    xSemaphoreGive(semNVSEntry);
    return true;
}

bool Indication::saveVariablesToNVS(void)
{
    if (show & _showNVS)
        ESP_LOGW(TAG, "saveVariablesToNVS");

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY) == pdTRUE)
    {
        if unlikely (nvs->openNVSStorage("indication", true) == false)
        {
            ESP_LOGE(TAG, "Error, Unable to OpenNVStorage inside saveVariablesToNVS");
            xSemaphoreGive(semNVSEntry);
            return false;
        }
    }

    bool successFlag = true;

    if (successFlag) // Save runStackSizeK
    {
        if (nvs->saveU8IntegerToNVS("runStackSizeK", runStackSizeK))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): runStackSizeK = " + std::to_string(runStackSizeK));
        }
        else
        {
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save runStackSizeK");
            successFlag = false;
        }
    }

    //
    // Save Color States
    //
    if (successFlag) // Save aState
    {
        if (aState_nvs_dirty)
        {
            if (nvs->saveU8IntegerToNVS("aState", (int)aState))
            {
                if (show & _showNVS)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aState = " + std::to_string((int)aState));
                aState_nvs_dirty = false;
            }
            else
            {
                routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save aState");
                successFlag = false;
            }
        }
    }

    if (successFlag) // Save bState
    {
        if (bState_nvs_dirty)
        {
            if (nvs->saveU8IntegerToNVS("bState", (int)bState))
            {
                if (show & _showNVS)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bState = " + std::to_string((int)bState));
                bState_nvs_dirty = false;
            }
            else
            {
                routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save bState");
                successFlag = false;
            }
        }
    }

    if (successFlag) // Save cState
    {
        if (cState_nvs_dirty)
        {
            if (nvs->saveU8IntegerToNVS("cState", (int)cState))
            {
                if (show & _showNVS)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cState = " + std::to_string((int)cState));
                cState_nvs_dirty = false;
            }
            else
            {
                routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save cState");
                successFlag = false;
            }
        }
    }

    //
    // Save Default Color Values
    //
    if (successFlag) // Save aDefaultValue
    {
        if (aDefaultValue_nvs_dirty)
        {
            if (nvs->saveU8IntegerToNVS("aDefaultValue", (int)aDefaultValue))
            {
                if (show & _showNVS)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aDefaultValue = " + std::to_string((int)aDefaultValue));
                aDefaultValue_nvs_dirty = false;
            }
            else
            {
                routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save aDefaultValue");
                successFlag = false;
            }
        }
    }

    if (successFlag) // Save bDefaultValue
    {
        if (bDefaultValue_nvs_dirty)
        {
            if (nvs->saveU8IntegerToNVS("bDefaultValue", (int)bDefaultValue))
            {
                if (show & _showNVS)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bDefaultValue = " + std::to_string((int)bDefaultValue));
                bDefaultValue_nvs_dirty = false;
            }
            else
            {
                routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save bDefaultValue");
                successFlag = false;
            }
        }
    }

    if (successFlag) // Save cDefaultValue
    {
        if (cDefaultValue_nvs_dirty)
        {
            if (nvs->saveU8IntegerToNVS("cDefaultValue", (int)cDefaultValue))
            {
                if (show & _showNVS)
                    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cDefaultValue = " + std::to_string((int)cDefaultValue));
                cDefaultValue_nvs_dirty = false;
            }
            else
            {
                routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save cDefaultValue");
                successFlag = false;
            }
        }
    }

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): indication end");

    if (successFlag)
    {
        if (show & _showNVS)
            routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Success");
        nvs->closeNVStorage(true); // Commit changes
    }
    else
    {
        routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Failed");
        nvs->closeNVStorage(false); // No changes
    }

    xSemaphoreGive(semNVSEntry);
    return true;
}
