#include "indication/indication_.hpp"

/* Local Semaphore */
SemaphoreHandle_t semIndRouteLock = NULL;

//
// Routing has been added to the project because Log messages will be sent to cloud monitoring and storage services in the future.
// This project does not include IOT services yet.
// ALSO, we have a plan (impliemented in more advanced work) to store Error messages in NVS so they can be reviewed even after a reboot.
//

/* Logging */
void Indication::routeLogByRef(LOG_TYPE _type, std::string *_msg)
{
    routeLogByValue(_type, *_msg); // Make a copy and see if this will work for all use cases where the msg is large.

    if (xSemaphoreTake(semIndRouteLock, portMAX_DELAY)) // We use this lock to prevent sys_evt and wifi_run tasks from having conflicts
    {
        LOG_TYPE type = _type;   // Copy our parameters upon entry before they are over-written by another calling task.
        std::string *msg = _msg; // This will point back to the caller's variable.

        switch (type)
        {
        case LOG_TYPE::ERROR:
        {
            ESP_LOGE(TAG, "%s", (*msg).c_str()); // Print out our errors here so we see it in the console.
            break;
        }

        case LOG_TYPE::WARN:
        {
            ESP_LOGW(TAG, "%s", (*msg).c_str()); // Print out our warning here so we see it in the console.
            break;
        }

        case LOG_TYPE::INFO:
        {
            ESP_LOGI(TAG, "%s", (*msg).c_str()); // Print out our information here so we see it in the console.
            break;
        }
        }

        xSemaphoreGive(semIndRouteLock);
    }
}

void Indication::routeLogByValue(LOG_TYPE _type, std::string _msg)
{
    if (xSemaphoreTake(semIndRouteLock, portMAX_DELAY)) // We use this lock to prevent sys_evt and wifi_run tasks from having conflicts
    {
        LOG_TYPE type = _type; // Copy our parameters upon entry before they are over-written by another calling task.
        std::string msg = _msg;

        switch (type)
        {
        case LOG_TYPE::ERROR:
        {
            ESP_LOGE(TAG, "%s", (msg).c_str()); // Print out our errors here so we see it in the console.
            break;
        }

        case LOG_TYPE::WARN:
        {
            ESP_LOGW(TAG, "%s", (msg).c_str()); // Print out our warning here so we see it in the console.
            break;
        }

        case LOG_TYPE::INFO:
        {
            ESP_LOGI(TAG, "%s", (msg).c_str()); // Print out our information here so we see it in the console.
            break;
        }
        }

        xSemaphoreGive(semIndRouteLock);
    }
}
