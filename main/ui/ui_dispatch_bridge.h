#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "dispatcher.h"
#include "dispatcher/dispatcher_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ui_dispatch_bridge_init(void);
esp_err_t ui_dispatch_bridge_deinit(void);

esp_err_t ui_dispatch_bridge_send_params(const dispatcher_pool_send_params_t *params);

esp_err_t ui_dispatch_bridge_send_text(dispatch_source_t source,
                                       dispatch_target_t target,
                                       const char *text);

#ifdef __cplusplus
}
#endif
