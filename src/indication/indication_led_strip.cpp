#include "indication/indication_.hpp"

#include "driver/rmt_types.h"
#include "driver/rmt_tx.h"

/* LED Strip Routines */
esp_err_t Indication::rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    ESP_RETURN_ON_ERROR(rmt_del_encoder(led_encoder->bytes_encoder), "IND", "rmt_del_encoder() failed.");
    ESP_RETURN_ON_ERROR(rmt_del_encoder(led_encoder->copy_encoder), "IND", "rmt_del_encoder() failed.");
    free(led_encoder);
    return ESP_OK;
}

size_t Indication::rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (led_encoder->state)
    {
    case 0:
    {
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE)
        {
            led_encoder->state = 1; // switch to next state when current encoding session finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL)
        {
            state = (rmt_encode_state_t)(state | RMT_ENCODING_MEM_FULL);
            break; // yield if there's no free space for encoding artifacts
        }
        [[fallthrough]];
    }

    case 1:
    {
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code, sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE)
        {
            led_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
            state = (rmt_encode_state_t)(state | RMT_ENCODING_COMPLETE);
        }
        if (session_state & RMT_ENCODING_MEM_FULL)
        {
            state = (rmt_encode_state_t)(state | RMT_ENCODING_MEM_FULL);
            break; // yield if there's no free space for encoding artifacts
        }
        break;
    }
    }

    *ret_state = state;
    return encoded_symbols;
}

esp_err_t Indication::rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    ESP_RETURN_ON_ERROR(rmt_encoder_reset(led_encoder->bytes_encoder), "IND", "rmt_encoder_reset() failed.");
    ESP_RETURN_ON_ERROR(rmt_encoder_reset(led_encoder->copy_encoder), "IND", "rmt_encoder_reset() failed.");
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

esp_err_t Indication::rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;

    if (config == nullptr)
    {
        ESP_LOGE(TAG, "config encoder parameter can not be null...");
        return ESP_FAIL;
    }

    if unlikely (ret_encoder == nullptr)
    {
        ESP_LOGE(TAG, "ret_encoder handle parameter can not be null...");
        return ESP_FAIL;
    }

    rmt_led_strip_encoder_t *led_encoder = (rmt_led_strip_encoder_t *)calloc(1, sizeof(rmt_led_strip_encoder_t));

    if unlikely (led_encoder == nullptr)
    {
        ESP_LOGE(TAG, "Memory for rmt_led_strip_encoder_t allocation failed...");
        return ESP_FAIL;
    }

    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;

    // different led strip might have its own timing requirements, following parameter is for WS2812

    uint16_t bit0_duration0 = 0.3 * config->resolution / 1000000; // T1H=0.9us
    uint16_t bit0_duration1 = 0.9 * config->resolution / 1000000; // T0L=0.9us
    uint16_t bit1_duration0 = 0.9 * config->resolution / 1000000; // T1H=0.9us
    uint16_t bit1_duration1 = 0.3 * config->resolution / 1000000; // T1L=0.3us

    rmt_bytes_encoder_config_t bytes_encoder_config = {
        {bit0_duration0, 1, bit0_duration1, 0}, // bit0_duration0, bit0_level0, bit0_duration1, bit0_level1
        {bit1_duration0, 1, bit1_duration1, 0}, // bit1_duration0, bit1_level0, bit1_duration1, bit1_level1
        1,                                      // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder);

    if unlikely (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s::rmt_new_bytes_encoder() failed.  err = %d - %s", __func__, ret, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    rmt_copy_encoder_config_t copy_encoder_config = {};

    ret = rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder);

    if unlikely (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s::rmt_new_copy_encoder() failed.  err = %d - %s", __func__, ret, esp_err_to_name(ret));

        if (led_encoder->bytes_encoder) // Clean up the encoder from previous area
            rmt_del_encoder(led_encoder->bytes_encoder);

        return ESP_FAIL;
    }
    uint32_t reset_ticks = config->resolution / 1000000 * 50 / 2; // reset code duration defaults to 50us

    led_encoder->reset_code = (rmt_symbol_word_t){
        {(uint16_t)reset_ticks, 0, (uint16_t)reset_ticks, 0},
    };
    *ret_encoder = &led_encoder->base;

    return ESP_OK;
}