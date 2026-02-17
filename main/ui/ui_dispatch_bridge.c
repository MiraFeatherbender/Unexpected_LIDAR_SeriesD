#include "ui_dispatch_bridge.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "ui_dispatch_bridge";
static bool s_initialized = false;

esp_err_t ui_dispatch_bridge_init(void)
{
    s_initialized = true;
    return ESP_OK;
}

esp_err_t ui_dispatch_bridge_deinit(void)
{
    s_initialized = false;
    return ESP_OK;
}

esp_err_t ui_dispatch_bridge_send_params(const dispatcher_pool_send_params_t *params)
{
    if (!params) return ESP_ERR_INVALID_ARG;
    if (!params->targets) return ESP_ERR_INVALID_ARG;
    if (!params->data && params->data_len > 0) return ESP_ERR_INVALID_ARG;

    if (!s_initialized) {
        esp_err_t err = ui_dispatch_bridge_init();
        if (err != ESP_OK) return err;
    }

    pool_msg_t *sent = dispatcher_pool_send_ptr_params(params);
    if (!sent) return ESP_ERR_NO_MEM;

    return ESP_OK;
}

esp_err_t ui_dispatch_bridge_send_text(dispatch_source_t source,
                                       dispatch_target_t target,
                                       const char *text)
{
    if (target >= TARGET_MAX) return ESP_ERR_INVALID_ARG;
    if (!text) return ESP_ERR_INVALID_ARG;

    dispatch_target_t targets[TARGET_MAX];
    dispatcher_fill_targets(targets);
    targets[0] = target;

    dispatcher_pool_send_params_t params = {
        .type = DISPATCHER_POOL_STREAMING,
        .source = source,
        .targets = targets,
        .data = (const uint8_t *)text,
        .data_len = strlen(text),
        .context = NULL,
    };

    return ui_dispatch_bridge_send_params(&params);
}
