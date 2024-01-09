#pragma once
#include "indication/indication_defs.hpp"

#include <string> // Native Libraries

#include "freertos/FreeRTOS.h" // RTOS Libraries
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/projdefs.h"

#include "esp_check.h" // ESP Libraries
#include "esp_log.h"

#include "system_.hpp" // Component Libraries

class System; // Class Declarations
class NVS;

extern "C"
{
    class Indication
    {
    public:
        Indication(uint8_t, uint8_t, uint8_t);
        ~Indication();

        TaskHandle_t &getRunTaskHandle(void);
        QueueHandle_t &getCmdRequestQueue(void);

        /* Diagnostics */
        void printTaskInfo();

    private:
        char TAG[5] = "_ind";
        System *sys = nullptr;
        NVS *nvs = nullptr;

        uint8_t majorVer; // Used to flash the firmware version number on start-up.
        uint8_t minorVer;
        uint8_t patchNumber;

        uint8_t runStackSizeK = 5; // Default/Minimum stacksize

        uint8_t show = 0; // Flags

        void setShowFlags(); // Pre Task Functions
        void setLogLevels(void);
        void createSemaphores(void);
        void setConditionalCompVariables(void);

        /* freeRTOS Related Items*/
        IND_OP indOP = IND_OP::Run;
        IND_INIT initIndStep = IND_INIT::Finished;
        IND_STATES indStates = IND_STATES::Idle;

        TaskHandle_t taskHandleRun = nullptr;
        QueueHandle_t queHandleIndCmdRequest = nullptr; // IND <-- ?? (Request Queue is here)

        /* State Transition Variables */
        IND_NOTIFY indTaskNotifyValue = IND_NOTIFY::NONE;

        // ColorA is Red
        // ColorB is Green
        // ColorC is Blue

        /* Indication Variables */
        LED_STATE aState = LED_STATE::AUTO; // These are default Color States
        LED_STATE bState = LED_STATE::AUTO;
        LED_STATE cState = LED_STATE::AUTO;

        uint8_t aDefaultLevel = 1; // Default values are minimum brightness levels.  Without minimums, if the LED doesn't work,
        uint8_t bDefaultLevel = 1; // we won't know what the problem is during an "on" time.  CurrValues and SetValues will never
        uint8_t cDefaultLevel = 1; // be allowed to drop below these minumums.  Default values are set for hardware when built and tested.

        uint8_t aSetLevel = 0; // Set values are recorded in nvs and restored on start-up
        uint8_t bSetLevel = 0;
        uint8_t cSetLevel = 0;

        uint8_t aCurrValue = 0; // Curent values are not preserved
        uint8_t bCurrValue = 0;
        uint8_t cCurrValue = 0;

        uint8_t clearLEDTargets;
        uint8_t setLEDTargets;

        uint8_t first_color_target;
        uint8_t first_color_cycles;

        uint8_t second_color_target;
        uint8_t second_color_cycles;

        uint8_t off_time;
        uint8_t off_time_counter;

        uint8_t on_time;
        uint8_t on_time_counter;

        uint8_t first_on_time_counter;
        uint8_t second_on_time_counter;

        bool IsIndicating = false;

        static void runMarshaller(void *);
        void run(void);

        void startIndication(uint32_t);
        void setAndClearColors(uint8_t, uint8_t);
        void resetIndication(void);

        /* LED Strip Items */
        rmt_channel_handle_t led_chan = NULL;
        rmt_encoder_handle_t led_encoder = NULL;

        rmt_transmit_config_t tx_config = {
            0,      // Specify the times of transmission in a loop, -1 means transmitting in an infinite loop
            {0, 1}, // eot_level "End Of Transmission" level, Transaction Queue blocking (1 = return when full)
        };

        esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder);
        static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state);
        static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder);
        static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder);

        /* Logging */
        std::string errMsg = ""; // Variable used for "ByValue" call.
        void routeLogByRef(LOG_TYPE, std::string *);
        void routeLogByValue(LOG_TYPE, std::string);

        /* NVS Items */
        uint8_t saveToNVSDelayCount = 0;

        bool restoreVariablesFromNVS(void);
        bool saveVariablesToNVS(void);

        /* Utilities */
        std::string getStateText(LED_STATE);
    };
}
