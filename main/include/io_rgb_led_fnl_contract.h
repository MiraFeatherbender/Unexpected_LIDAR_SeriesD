#ifndef IO_RGB_LED_FNL_CONTRACT_H
#define IO_RGB_LED_FNL_CONTRACT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "FastNoiseLite.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RGB_LED_FNL_OP_GET_SNAPSHOT = 1,
    RGB_LED_FNL_OP_SET_SNAPSHOT = 2,
    RGB_LED_FNL_OP_ACK = 3,
} rgb_led_fnl_op_t;

typedef enum {
    RGB_LED_FNL_OK = 0,
    RGB_LED_FNL_ERR_ARG,
    RGB_LED_FNL_ERR_SIZE,
    RGB_LED_FNL_ERR_BUSY,
    RGB_LED_FNL_ERR_VERSION_OLD,
} rgb_led_fnl_status_t;

typedef struct {
    rgb_led_fnl_op_t op;
    uint32_t version;
    size_t state_size;
    fnl_state *state;
    SemaphoreHandle_t sem;
    rgb_led_fnl_status_t status;
    void *user_data;
} rgb_led_fnl_ctx_t;

static inline bool rgb_led_fnl_version_is_newer(uint32_t candidate, uint32_t reference)
{
    return (int32_t)(candidate - reference) > 0;
}

#ifdef __cplusplus
}
#endif

#endif // IO_RGB_LED_FNL_CONTRACT_H
