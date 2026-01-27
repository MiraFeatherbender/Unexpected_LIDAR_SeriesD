#ifndef IO_ULTRASONIC_H
#define IO_ULTRASONIC_H

#include <stdint.h>
#include "dispatcher.h"
#include "dispatcher_pool.h"
#include "dispatcher_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Public API: initialize the ultrasonic plugin */
void io_ultrasonic_init(void);

/** Optional: trigger a single measurement on demand */
void io_ultrasonic_trigger_once(void);

/**
 * Convenience macro: publish a millimeter value via the dispatcher pointer-pool.
 * Usage:
 *   dispatch_target_t targets[TARGET_MAX];
 *   dispatcher_fill_targets(targets);
 *   targets[0] = TARGET_LOG; // or TARGET_ULTRASONIC
 *   IO_ULTRASONIC_PUBLISH_MM(mm_val, targets);
 */
#define IO_ULTRASONIC_PUBLISH_MM(mm_value, target_array) \
    do { \
        dispatcher_pool_send_params_t _params = { \
            .type = DISPATCHER_POOL_STREAMING, \
            .source = SOURCE_ULTRASONIC, \
            .targets = (target_array), \
            .data = (const uint8_t *)&(mm_value), \
            .data_len = sizeof(mm_value), \
            .context = NULL \
        }; \
        dispatcher_pool_send_ptr_params(&_params); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif // IO_ULTRASONIC_H
