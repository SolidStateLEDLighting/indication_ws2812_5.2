#pragma once
#include "indication/indication_defs.hpp"

#include <string> // Native Libraries

#include "freertos/FreeRTOS.h" // RTOS Libraries
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/projdefs.h"

#include "esp_log.h" // ESP Libraries
#include "driver/rmt_types.h"
#include "driver/rmt_tx.h"

#include "system_.hpp"
#include "nvs/nvs_.hpp"

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
        void printTaskInfo();

    private:
        char TAG[5] = "_ind";
        System *sys = nullptr;
        NVS *nvs = nullptr;

        uint8_t runStackSizeK = 5; // Default size

        /* Debugging Items */
        uint8_t show = 0;

        void setShowFlags();
        void setLogLevels(void);
        void createSemaphores(void);
        void setConditionalCompVariables(void);

        /* LED Strip Items */
        rmt_channel_handle_t led_chan = NULL;
        rmt_encoder_handle_t led_encoder = NULL;

        rmt_transmit_config_t tx_config = {
            0,   // Specify the times of transmission in a loop, -1 means transmitting in an infinite loop
            {0}, // eot_level "End Of Transmission" level
        };

        uint8_t majorVer;
        uint8_t minorVer;
        uint8_t revNumber;

        LED_STATE aState = LED_STATE::AUTO; // This is a Color State
        LED_STATE bState = LED_STATE::AUTO;
        LED_STATE cState = LED_STATE::AUTO;

        bool aState_nvs_dirty = false;
        bool bState_nvs_dirty = false;
        bool cState_nvs_dirty = false;

        bool aState_iot_dirty = false;
        bool bState_iot_dirty = false;
        bool cState_iot_dirty = false;

        uint8_t aCurrValue = 0; // Curent values are not preserved
        uint8_t bCurrValue = 0;
        uint8_t cCurrValue = 0;

        uint8_t aDefaultValue = 5; // Default values are brightness levels
        uint8_t bDefaultValue = 10;
        uint8_t cDefaultValue = 50;

        bool aDefaultValue_nvs_dirty = false;
        bool bDefaultValue_nvs_dirty = false;
        bool cDefaultValue_nvs_dirty = false;

        IND_OP indOP = IND_OP::Run;
        IND_INIT initINDStep = IND_INIT::Finished;
        IND_STATES indStates = IND_STATES::Init;

        TaskHandle_t taskHandleRun = nullptr;

        QueueHandle_t queHandleINDCmdRequest = nullptr; // IND <-- ?? (Request Queue is here)

        IND_NOTIFY indTaskNotifyValue = IND_NOTIFY::NONE;

        bool IsIndicating = false;

        /* NVS Variables*/
        uint8_t saveToNVSDelayCount = 0;

        /* Indication Variables */
        uint8_t clearLEDTargets;
        uint8_t setLEDTargets;

        uint8_t off_time;
        uint8_t off_time_counter;

        uint8_t first_color_target;
        uint8_t first_color_cycles;

        uint8_t second_color_target;
        uint8_t second_color_cycles;

        uint8_t on_time;
        uint8_t on_time_counter;

        uint8_t first_on_time_counter;
        uint8_t second_on_time_counter;

        static void runMarshaller(void *);
        void run(void);

        esp_err_t startIndication(uint32_t);
        void setAndClearColors(uint8_t, uint8_t);
        void resetIndication(void);

        bool restoreVariablesFromNVS(void);
        bool saveVariablesToNVS(void);

        esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder);
        static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state);
        static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder);
        static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder);

        std::string getStateText(uint8_t);

        void routeLogByRef(LOG_TYPE, std::string *);
        void routeLogByValue(LOG_TYPE, std::string);
    };
}
