#include "indication/indication_.hpp"

SemaphoreHandle_t semIndEntry = NULL;

/* External Variables */
extern SemaphoreHandle_t semSysEntry;
extern SemaphoreHandle_t semNVSEntry;
extern SemaphoreHandle_t semIndRouteLock;

/* Construction/Destruction */
Indication::Indication(uint8_t myMajorVer, uint8_t myMinorVer, uint8_t myPatchNumber)
{
    // Process of creating this object:
    // 1) Copy parameters into object variables.
    // 2) Get the system run task handle
    // 3) Set the show flags.
    // 4) Set log levels
    // 5) Create all the semaphores
    // 6) Restore all the object variables from nvs.
    // 7) Lock the object with its entry semaphore.
    // 8) Start this object's run task.
    // 9) Done.

    majorVer = myMajorVer;       // We pass in the software version of the project so our LEDs can flash this number during startup
    minorVer = myMinorVer;       //
    patchNumber = myPatchNumber; //

    if (xSemaphoreTake(semSysEntry, portMAX_DELAY)) // Get everything from the system that we need.
    {
        if (sys == nullptr)
            sys = System::getInstance();
        xSemaphoreGive(semSysEntry);
    }

    setShowFlags();            // Static enabling of logging statements for any area of concern during development.
    setLogLevels();            // Manually sets log levels for tasks down the call stack for development.
    createSemaphores();        // Creates any locking semaphores owned by this object.
    restoreVariablesFromNVS(); // Brings back all our persistant data.

    xSemaphoreTake(semIndEntry, portMAX_DELAY); // Take the semaphore.  This gives us a locking mechanism for initialization.

    initIndStep = IND_INIT::Start; // Allow the object to initialize and then run.
    indOP = IND_OP::Init;
    xTaskCreate(runMarshaller, "ind_run", 1024 * runStackSizeK, this, TASK_PRIORITY_OFFSET_LOW, &taskHandleRun); // Low number indicates low priority task
}

Indication::~Indication()
{
    // Process of destroying this object:
    // 1) Lock the object with its entry semaphore.
    // 2) Send a task notification to CMD_SHUT_DOWN.
    // 3) Watch the runTaskHandle and wait for it to clean up and then become nullptr.
    // 4) Clean up other resources created by calling task from the constructor.
    // 5) UnLock the its entry semaphore.
    // 6) Destroy all semaphores at the same time (for good organization). Again, these are created by calling task in constructor.
    // 7) Done.

    // The calling task can still send taskNotifications to the indication task!
    while (!xTaskNotify(taskHandleRun, static_cast<uint32_t>(IND_NOTIFY::CMD_SHUT_DOWN), eSetValueWithoutOverwrite))
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the notification to be received.
    taskYIELD();                       // One last yeild to make sure Idle task can run.

    while (taskHandleRun != nullptr)
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the wifi task handle to become null.
    taskYIELD();                       // One last yeild to make sure Idle task can run.

    xSemaphoreGive(semIndEntry);
    destroySemaphores();
}

/* Construction Functions */
void Indication::setShowFlags()
{
    // show variable is system wide defined and this exposes for viewing any general processes.
    show = 0; // Set show flags
    // show |= _showInit;
    // show |= _showNVS;
    // show |= _showRun;
    // show |= _showEvents;
    // show |= _showJSONProcessing;
    // show |=  _showDebugging
    // show |=  _showProcess
    // show |=  _showPayload

    // showIND exposes indication sub-processes.
    showIND = 0;
    // showIND |= _showINDShdnSteps;
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
        xSemaphoreGive(semIndEntry);

    semIndRouteLock = xSemaphoreCreateBinary();
    if (semIndRouteLock != NULL)
        xSemaphoreGive(semIndRouteLock);
}

void Indication::destroySemaphores()
{
    if (semIndEntry != nullptr)
    {
        vSemaphoreDelete(semIndEntry);
        semIndEntry = nullptr;
    }

    if (semIndRouteLock != nullptr)
    {
        vSemaphoreDelete(semIndRouteLock);
        semIndRouteLock = nullptr;
    }
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
