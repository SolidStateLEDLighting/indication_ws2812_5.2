#include "indication/indication_.hpp"

SemaphoreHandle_t semIndEntry = NULL;

/* External Variables */
extern SemaphoreHandle_t semSysEntry;
extern SemaphoreHandle_t semNVSEntry;
extern SemaphoreHandle_t semIndRouteLock;

/* Construction/Destruction */
Indication::Indication(uint8_t myMajorVer, uint8_t myMinorVer, uint8_t myPatchNumber)
{
    majorVer = myMajorVer; // We pass in the software version of the project so out LEDs
    minorVer = myMinorVer; // can flash this out during startup
    patchNumber = myPatchNumber;

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

    initIndStep = IND_INIT::Start; // Allow the object to initialize.
    indOP = IND_OP::Init;

    xTaskCreate(runMarshaller, "ind_run", 1024 * runStackSizeK, this, 6, &taskHandleRun); // Low number indicates low priority task
}

Indication::~Indication()
{
    if (queHandleIndCmdRequest != nullptr)
        vQueueDelete(queHandleIndCmdRequest);

    if (semIndEntry != NULL)
        vSemaphoreDelete(semIndEntry);

    if (this->taskHandleRun != nullptr)
        vTaskDelete(NULL);
}

/* Construction Functions */
void Indication::setShowFlags()
{
    show = 0; // Set show flags
    // show |= _showInit;
    // show |= _showNVS;
    // show |= _showRun;
    // show |= _showEvents;
    // show |= _showJSONProcessing;
    // show |=  _showDebugging
    // show |=  _showProcess
    // show |=  _showPayload
}

void Indication::setLogLevels()
{
    if (show > 0)                             // Normally, we are interested in the variables inside our object.
        esp_log_level_set(TAG, ESP_LOG_INFO); // If we have any flags set, we need to be sure to turn on the logging so we can see them.
    else
        esp_log_level_set(TAG, ESP_LOG_ERROR); // Alturnatively, we turn down logging for Errors only, if we are not looking for anything else.

    // If we notice any supporting libraries printing to the serial output, we can alter their logging levels here in development.
}

void Indication::createSemaphores()
{
    semIndEntry = xSemaphoreCreateBinary(); // External access semaphore

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

/* Public Functions */
TaskHandle_t &Indication::getRunTaskHandle(void)
{
    return taskHandleRun;
}

QueueHandle_t &Indication::getCmdRequestQueue(void)
{
    return queHandleIndCmdRequest;
}
