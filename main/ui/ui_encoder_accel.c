#include "ui_encoder_accel.h"

static float ui_encoder_accel_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float ui_encoder_accel_clampf(float value, float minv, float maxv)
{
    if (value < minv) return minv;
    if (value > maxv) return maxv;
    return value;
}

void ui_encoder_accel_reset(ui_encoder_accel_t *cfg)
{
    if (!cfg) return;
    cfg->accum = 0.0f;
    cfg->residual = 0.0f;
    cfg->last_ms = 0;
}

int32_t ui_encoder_accel_apply(ui_encoder_accel_t *cfg, int8_t dir, int64_t now_ms)
{
    if (!cfg || dir == 0) return 0;

    if (cfg->last_ms == 0) cfg->last_ms = now_ms;

    int32_t dt_ms = (int32_t)(now_ms - cfg->last_ms);
    cfg->last_ms = now_ms;
    if (dt_ms < 5) dt_ms = 5;
    if (dt_ms > 300) dt_ms = 300;

    float decay = (cfg->tau_ms > 1.0f) ? ((float)dt_ms / cfg->tau_ms) : 1.0f;
    decay = ui_encoder_accel_clampf(decay, 0.0f, 1.0f);
    cfg->accum -= (cfg->accum * decay);

    float velocity = (ui_encoder_accel_absf((float)dir) * 1000.0f) / (float)dt_ms;
    float norm_accum = (cfg->accel_max > 0.0f) ? (cfg->accum / cfg->accel_max) : 0.0f;
    norm_accum = ui_encoder_accel_clampf(norm_accum, 0.0f, 1.0f);
    float momentum_boost = 1.0f + (0.8f * norm_accum);

    cfg->accum += cfg->gain_k * velocity * momentum_boost;
    cfg->accum = ui_encoder_accel_clampf(cfg->accum, 0.0f, cfg->accel_max);

    float scaled = ((float)dir * cfg->base_step * (1.0f + cfg->accum)) + cfg->residual;
    int32_t whole = (int32_t)scaled;
    cfg->residual = scaled - (float)whole;
    return whole;
}
