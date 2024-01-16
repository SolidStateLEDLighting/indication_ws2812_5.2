#include "indication/indication_.hpp"

/* Diagnostics */
void Indication::printTaskInfoByColumns()
{
    char *name = pcTaskGetName(taskHandleRun);
    uint32_t priority = uxTaskPriorityGet(taskHandleRun);
    uint32_t highWaterMark = uxTaskGetStackHighWaterMark(taskHandleRun);
    printf("  %-10s   %02ld           %ld\n", name, priority, highWaterMark);
}

void Indication::logTaskInfo()
{
    char *name = pcTaskGetName(NULL); // Note: The value of NULL can be used as a parameter if the statement is running on the task of your inquiry.
    uint32_t priority = uxTaskPriorityGet(NULL);
    uint32_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
    routeLogByValue(LOG_TYPE::INFO, std::string(__func__) + "(): name: " + std::string(name) + " priority: " + std::to_string(priority) + " highWaterMark: " + std::to_string(highWaterMark));
}
