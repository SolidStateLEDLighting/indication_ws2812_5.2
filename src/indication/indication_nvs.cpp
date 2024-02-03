#include "indication/indication_.hpp"

#include "nvs/nvs_.hpp" // Our components

#include "esp_check.h"

/* External Semaphores */
extern SemaphoreHandle_t semNVSEntry;

/* NVS Routines */
void Indication::restoreVariablesFromNVS(void)
{
    esp_err_t ret = ESP_OK;
    bool successFlag = true;
    uint8_t temp = 0;

    if (nvs == nullptr)
        nvs = NVS::getInstance(); // First, get the nvs object handle if didn't already.

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY))
        ESP_GOTO_ON_ERROR(nvs->openNVSStorage("indication"), ind_restoreVariablesFromNVS_err, TAG, "nvs->openNVSStorage('indication') failed");

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): indication namespace start");

    if (successFlag) // Restore runStackSizeK
    {
        temp = runStackSizeK;
        ret = nvs->readU8IntegerFromNVS("runStackSizeK", &temp); // This will save the default size if that value doesn't exist yet in nvs.

        if (ret == ESP_OK)
        {
            if (temp > runStackSizeK) // Ok to use any value greater than the default size.
            {
                runStackSizeK = temp;
                ret = nvs->writeU8IntegerToNVS("runStackSizeK", runStackSizeK); // Over-write the value with the default minumum value.
            }

            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): runStackSizeK       is " + std::to_string(runStackSizeK));
        }

        if (ret != ESP_OK)
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore runStackSizeK");
        }
    }

    if (successFlag) // Restore aState
    {
        if (nvs->readU8IntegerFromNVS("aState", (uint8_t *)&aState) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aState              is " + getStateText(aState));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore aState");
        }
    }

    if (successFlag) // Restore bState
    {
        if (nvs->readU8IntegerFromNVS("bState", (uint8_t *)&bState) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bState              is " + getStateText(bState));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore bState");
        }
    }

    if (successFlag) // Restore cState
    {
        if (nvs->readU8IntegerFromNVS("cState", (uint8_t *)&cState) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cState              is " + getStateText(cState));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore cState");
        }
    }

    if (successFlag) // Restore aSetLevel
    {
        if (aSetLevel < aDefaultLevel) // Bring SetLevel up to DefaultLevel if needed
            aSetLevel = aDefaultLevel;

        ret = nvs->readU8IntegerFromNVS("aSetLevel", (uint8_t *)&aSetLevel); // Value will be saved if variable in nvs is empty.

        if (ret == ESP_OK) // Result of restore, or possibly set the inital default value.
        {
            if (aSetLevel < aDefaultLevel) // Make sure SetLevel is equal or above DefaultLevel again.
            {
                aSetLevel = aDefaultLevel;
                ret = nvs->writeU8IntegerToNVS("aSetLevel", aSetLevel);
            }

            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aSetLevel           is " + std::to_string(aSetLevel));
        }

        if (ret != ESP_OK)
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore aSetLevel");
        }
    }

    if (successFlag) // Restore bSetLevel
    {
        if (bSetLevel < bDefaultLevel) // Bring SetLevel up to DefaultLevel if needed
            bSetLevel = bDefaultLevel;

        ret = nvs->readU8IntegerFromNVS("bSetLevel", (uint8_t *)&bSetLevel); // Value will be saved if variable in nvs is empty.

        if (ret == ESP_OK) // Result of restore, or possibly set the inital default value.
        {
            if (bSetLevel < bDefaultLevel) // Make sure SetLevel is equal or above DefaultLevel again.
            {
                bSetLevel = bDefaultLevel;
                ret = nvs->writeU8IntegerToNVS("bSetLevel", bSetLevel);
            }

            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bSetLevel           is " + std::to_string(bSetLevel));
        }

        if (ret != ESP_OK)
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore bSetLevel");
        }
    }

    if (successFlag) // Restore cSetLevel
    {
        if (cSetLevel < cDefaultLevel) // Bring SetLevel up to DefaultLevel if needed
            cSetLevel = cDefaultLevel;

        ret = nvs->readU8IntegerFromNVS("cSetLevel", (uint8_t *)&cSetLevel); // Value will be saved if variable in nvs is empty.

        if (ret == ESP_OK) // Result of restore, or possibly set the inital default value.
        {
            if (cSetLevel < cDefaultLevel) // Make sure SetLevel is equal or above DefaultLevel again.
            {
                cSetLevel = cDefaultLevel;
                ret = nvs->writeU8IntegerToNVS("cSetLevel", cSetLevel);
            }

            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cSetLevel           is " + std::to_string(cSetLevel));
        }

        if (ret != ESP_OK)
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore cSetLevel");
        }
    }

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): indication namespace end");

    if (successFlag)
    {
        if (show & _showNVS)
            routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Success");
    }
    else
        routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Failed");

    nvs->closeNVStorage();
    xSemaphoreGive(semNVSEntry);
    return;

ind_restoreVariablesFromNVS_err:
    routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error " + esp_err_to_name(ret));
    xSemaphoreGive(semNVSEntry);
}

void Indication::saveVariablesToNVS(void)
{
    //
    // The best idea is to save only changed values.  Right now, we try to save everything.
    // The NVS object we call will avoid over-writing variables which already hold the identical values.
    // Later, we will add 'dirty' bits to avoid trying to save a value that hasn't changed.
    //
    esp_err_t ret = ESP_OK;
    bool successFlag = true;

    if (nvs == nullptr)
        nvs = NVS::getInstance(); // First, get the nvs object handle if didn't already.

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY))
        ESP_GOTO_ON_ERROR(nvs->openNVSStorage("indication"), ind_saveVariablesToNVS_err, TAG, "nvs->openNVSStorage('indication') failed");

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): indication namespace start");

    if (successFlag) // Save runStackSizeK
    {
        if (nvs->writeU8IntegerToNVS("runStackSizeK", runStackSizeK) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): runStackSizeK       = " + std::to_string(runStackSizeK));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save runStackSizeK");
        }
    }

    if (successFlag) // Save aState
    {
        if (nvs->writeU8IntegerToNVS("aState", (int)aState) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aState = " + std::to_string((int)aState));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save aState");
        }
    }

    if (successFlag) // Save bState
    {
        if (nvs->writeU8IntegerToNVS("bState", (int)bState) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bState = " + std::to_string((int)bState));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save bState");
        }
    }

    if (successFlag) // Save cState
    {
        if (nvs->writeU8IntegerToNVS("cState", (int)cState) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cState = " + std::to_string((int)cState));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save cState");
        }
    }

    if (successFlag) // Save aSetLevel
    {
        if (nvs->writeU8IntegerToNVS("aSetLevel", (int)aSetLevel) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aSetLevel = " + std::to_string((int)aSetLevel));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save aSetLevel");
        }
    }

    if (successFlag) // Save bSetLevel
    {
        if (nvs->writeU8IntegerToNVS("bSetLevel", (int)bSetLevel) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bSetLevel = " + std::to_string((int)bSetLevel));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save bSetLevel");
        }
    }

    if (successFlag) // Save cSetLevel
    {
        if (nvs->writeU8IntegerToNVS("cSetLevel", (int)cSetLevel) == ESP_OK)
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cSetLevel = " + std::to_string((int)cSetLevel));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save cSetLevel");
        }
    }

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): indication namespace end");

    if (successFlag)
    {
        if (show & _showNVS)
            routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): Success");
    }
    else
        routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Failed");

    nvs->closeNVStorage();
    xSemaphoreGive(semNVSEntry);
    return;

ind_saveVariablesToNVS_err:
    routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error " + esp_err_to_name(ret));
    xSemaphoreGive(semNVSEntry);
}
