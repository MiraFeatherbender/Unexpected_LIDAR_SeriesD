#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float base_step;
    float gain_k;
    float tau_ms;
    float accel_max;
    float accum;
    float residual;
    int64_t last_ms;
} ui_encoder_accel_t;

void ui_encoder_accel_reset(ui_encoder_accel_t *cfg);
int32_t ui_encoder_accel_apply(ui_encoder_accel_t *cfg, int8_t dir, int64_t now_ms);

#ifdef __cplusplus
}
#endif
