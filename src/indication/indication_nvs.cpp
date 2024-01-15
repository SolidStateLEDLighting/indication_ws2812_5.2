#include "indication/indication_.hpp"

#include "nvs/nvs_.hpp" // Our components

/* External Variables */
extern SemaphoreHandle_t semNVSEntry;

/* NVS Routines */
bool Indication::restoreVariablesFromNVS(void)
{
    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "()");

    uint8_t temp = 0;
    bool successFlag = true;

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY) == pdTRUE)
    {
        if (nvs == nullptr)
            nvs = NVS::getInstance(); // First, get the nvs object handle if didn't already.

        if (show & _showNVS)
            routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): openNVSStorage 'indication'");

        if (nvs->openNVSStorage("indication", true) == false)
        {
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "Error, Unable to OpenNVStorage inside restoreVariablesFromNVS");
            xSemaphoreGive(semNVSEntry);
            return false;
        }
    }

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

    if (successFlag) // Restore aState
    {
        if (nvs->getU8IntegerFromNVS("aState", (uint8_t *)&aState))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aState is " + getStateText(aState));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore aState");
        }
    }

    if (successFlag) // Restore bState
    {
        if (nvs->getU8IntegerFromNVS("bState", (uint8_t *)&bState))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bState is " + getStateText(bState));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore bState");
        }
    }

    if (successFlag) // Restore cState
    {
        if (nvs->getU8IntegerFromNVS("cState", (uint8_t *)&cState))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cState is " + getStateText(cState));
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

        if (nvs->getU8IntegerFromNVS("aSetLevel", (uint8_t *)&aSetLevel)) // Attempt to restore, or possibly set the inital default value.
        {
            if (aSetLevel < aDefaultLevel) // Make sure SetLevel is equal or above DefaultLevel
            {
                aSetLevel = aDefaultLevel;
                saveToNVSDelayCount = 10;
            }

            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aSetLevel is " + std::to_string(aSetLevel));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore aSetLevel");
        }
    }

    if (successFlag) // Restore bSetLevel
    {
        if (bSetLevel < bDefaultLevel) // Bring SetLevel up to DefaultLevel if needed
            bSetLevel = bDefaultLevel;

        if (nvs->getU8IntegerFromNVS("bSetLevel", (uint8_t *)&bSetLevel)) // Attempt to restore, or possibly set the inital default value.
        {
            if (bSetLevel < bDefaultLevel) // Make sure SetLevel is equal or above DefaultLevel
            {
                bSetLevel = bDefaultLevel;
                saveToNVSDelayCount = 10;
            }

            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bSetLevel is " + std::to_string(bSetLevel));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore bSetLevel");
        }
    }

    if (successFlag) // Restore cSetLevel
    {
        if (cSetLevel < cDefaultLevel) // Bring SetLevel up to DefaultLevel if needed
            cSetLevel = cDefaultLevel;

        if (nvs->getU8IntegerFromNVS("cSetLevel", (uint8_t *)&cSetLevel)) // Attempt to restore, or possibly set the inital default value.
        {
            if (cSetLevel < cDefaultLevel) // Make sure SetLevel is equal or above DefaultLevel
            {
                cSetLevel = cDefaultLevel;
                saveToNVSDelayCount = 10;
            }

            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cSetLevel is " + std::to_string(cSetLevel));
        }
        else
        {
            successFlag = false;
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Error, Unable to restore cSetLevel");
        }
    }

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): closeNVStorage 'indication'");

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

bool Indication::saveVariablesToNVS(void)
{
    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "()");

    bool successFlag = true;

    if (xSemaphoreTake(semNVSEntry, portMAX_DELAY) == pdTRUE)
    {
        if (show & _showNVS)
            routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): openNVSStorage 'indication'");

        if (nvs->openNVSStorage("indication", true) == false)
        {
            routeLogByValue(LOG_TYPE::ERROR, "Error, Unable to OpenNVStorage inside saveVariablesToNVS");
            xSemaphoreGive(semNVSEntry);
            return false;
        }
    }

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

    if (successFlag) // Save aState
    {
        if (nvs->saveU8IntegerToNVS("aState", (int)aState))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aState = " + std::to_string((int)aState));
        }
        else
        {
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save aState");
            successFlag = false;
        }
    }

    if (successFlag) // Save bState
    {
        if (nvs->saveU8IntegerToNVS("bState", (int)bState))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bState = " + std::to_string((int)bState));
        }
        else
        {
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save bState");
            successFlag = false;
        }
    }

    if (successFlag) // Save cState
    {
        if (nvs->saveU8IntegerToNVS("cState", (int)cState))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cState = " + std::to_string((int)cState));
        }
        else
        {
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save cState");
            successFlag = false;
        }
    }

    if (successFlag) // Save aSetLevel
    {
        if (nvs->saveU8IntegerToNVS("aSetLevel", (int)aSetLevel))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): aSetLevel = " + std::to_string((int)aSetLevel));
        }
        else
        {
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save aSetLevel");
            successFlag = false;
        }
    }

    if (successFlag) // Save bSetLevel
    {
        if (nvs->saveU8IntegerToNVS("bSetLevel", (int)bSetLevel))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): bSetLevel = " + std::to_string((int)bSetLevel));
        }
        else
        {
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save bSetLevel");
            successFlag = false;
        }
    }

    if (successFlag) // Save cSetLevel
    {
        if (nvs->saveU8IntegerToNVS("cSetLevel", (int)cSetLevel))
        {
            if (show & _showNVS)
                routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): cSetLevel = " + std::to_string((int)cSetLevel));
        }
        else
        {
            routeLogByValue(LOG_TYPE::ERROR, std::string(__func__) + "(): Unable to save cSetLevel");
            successFlag = false;
        }
    }

    if (show & _showNVS)
        routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): closeNVStorage 'indication'");

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
