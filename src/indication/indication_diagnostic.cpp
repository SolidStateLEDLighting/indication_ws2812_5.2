#include "indication/indication_.hpp"

/* Diagnostics */
void Indication::printTaskInfo()
{
    char *name = pcTaskGetName(taskHandleRun);
    uint32_t priority = uxTaskPriorityGet(taskHandleRun);
    uint32_t highWaterMark = uxTaskGetStackHighWaterMark(taskHandleRun);
    printf("  %-10s   %02ld           %ld\n", name, priority, highWaterMark);
}
