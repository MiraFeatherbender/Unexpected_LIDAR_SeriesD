#ifndef IO_RGB_SETTINGS_CONTRACT_V2_H
#define IO_RGB_SETTINGS_CONTRACT_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FastNoiseLite.h"
#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "rgb_anim.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Draft v2 settings contract
 *
 * Hybrid model:
 *  - Session control plane: GET_SNAPSHOT / BEGIN_EDIT / END_EDIT
 *  - Data plane: SET_WORKING (mailbox/latest-value semantics)
 *
 * This header defines message shapes only. Runtime behavior is implemented by
 * output modules (strip/onboard) and UI pages.
 */

typedef enum {
    RGB_SETTINGS_DOMAIN_FNL = 1,
    RGB_SETTINGS_DOMAIN_OFFSETS_3D = 2,
    RGB_SETTINGS_DOMAIN_WALK_RANGE = 3,
    RGB_SETTINGS_DOMAIN_HSV_TEST = 4,
    RGB_SETTINGS_DOMAIN_BRIGHTNESS_TEST = 5,
} rgb_settings_domain_t;

typedef enum {
    RGB_SETTINGS_PLUGIN_TYPE_RGB = 1u << 0,
    RGB_SETTINGS_PLUGIN_TYPE_HSV = 1u << 1,
    RGB_SETTINGS_PLUGIN_TYPE_ALL = RGB_SETTINGS_PLUGIN_TYPE_RGB | RGB_SETTINGS_PLUGIN_TYPE_HSV,
} rgb_settings_plugin_type_mask_t;

typedef enum {
    RGB_SETTINGS_OP_GET_SNAPSHOT = 1,
    RGB_SETTINGS_OP_BEGIN_EDIT = 2,
    RGB_SETTINGS_OP_SET_WORKING = 3,
    RGB_SETTINGS_OP_END_EDIT = 4,
    RGB_SETTINGS_OP_ACK = 5,
} rgb_settings_op_t;

typedef enum {
    RGB_SETTINGS_END_REASON_RELEASE = 0,
    RGB_SETTINGS_END_REASON_COMMIT = 1,
    RGB_SETTINGS_END_REASON_CANCEL = 2,
} rgb_settings_end_reason_t;

typedef enum {
    RGB_SETTINGS_OK = 0,
    RGB_SETTINGS_ERR_ARG,
    RGB_SETTINGS_ERR_SIZE,
    RGB_SETTINGS_ERR_BUSY,
    RGB_SETTINGS_ERR_ROUTE,
    RGB_SETTINGS_ERR_DOMAIN,
    RGB_SETTINGS_ERR_PLUGIN,
    RGB_SETTINGS_ERR_VERSION_OLD,
    RGB_SETTINGS_ERR_TIMEOUT,
} rgb_settings_status_t;

typedef enum {
    RGB_SETTINGS_SCOPE_ALL = 0,
    RGB_SETTINGS_SCOPE_GROUP_ID = 1,
    RGB_SETTINGS_SCOPE_LED_RANGE = 2,
    RGB_SETTINGS_SCOPE_MASK_ID = 3,
} rgb_settings_scope_type_t;

typedef struct {
    rgb_settings_scope_type_t type;
    union {
        uint16_t group_id;
        struct {
            uint16_t led_start;
            uint16_t led_end;
        } led_range;
        uint16_t mask_id;
    } selector;
} rgb_settings_scope_t;

typedef struct {
    float x_offset;
    float y_offset;
    float z_offset;
} rgb_settings_offsets_3d_t;

typedef struct {
    // Speed is the base offset applied to all points. 
    // Jitter is the additional random range around that speed (with jitter >= 0).
    float speed_dx;
    float jitter_dx;
    float speed_dy;
    float jitter_dy;
    float speed_dz;
    float jitter_dz;
} rgb_settings_walk_range_t;

typedef struct {
    hsv_color_t hsv;
} rgb_settings_hsv_test_t;

typedef struct {
    uint8_t brightness;
} rgb_settings_brightness_test_t;

typedef union {
    fnl_state fnl;
    rgb_settings_offsets_3d_t offsets_3d;
    rgb_settings_walk_range_t walk_range;
    rgb_settings_hsv_test_t hsv_test;
    rgb_settings_brightness_test_t brightness_test;
} rgb_settings_payload_u;

typedef struct {
    rgb_settings_op_t op;
    rgb_settings_domain_t domain;

    /*
     * Route target to apply/edit.
     * Example: TARGET_RGB_STRIP or TARGET_RGB_ONBOARD.
     */
    dispatch_target_t output_target;

    /* Scope target for future subgroup support. Default: RGB_SETTINGS_SCOPE_ALL. */
    rgb_settings_scope_t scope;

    /* Plugin context used for visibility/routing validation. */
    uint8_t plugin_id;
    uint32_t plugin_type_mask;

    /* Session ownership token (assigned/validated by receiver). */
    uint32_t session_token;

    /* Monotonic version for last-write-wins / stale-update rejection. */
    uint32_t version;

    /* Used by END_EDIT only. */
    rgb_settings_end_reason_t end_reason;

    /*
     * Generic payload pointer contract:
     * - payload points to a domain-appropriate struct
     * - payload_size must match that struct
     */
    void *payload;
    size_t payload_size;

    /* Optional sync semaphore for control-plane operations. */
    SemaphoreHandle_t sem;

    /* Filled by receiver before signaling sem (if provided). */
    rgb_settings_status_t status;

    /* Optional opaque caller context. */
    void *user_data;
} rgb_settings_ctx_t;

/* Mailbox helper shape for latest-value semantics in output modules. */
typedef struct {
    uint32_t session_token;
    uint32_t version;
    rgb_settings_domain_t domain;
    rgb_settings_scope_t scope;
    rgb_settings_payload_u payload;
} rgb_settings_mailbox_t;

static inline rgb_settings_scope_t rgb_settings_scope_all(void)
{
    rgb_settings_scope_t scope = {};
    scope.type = RGB_SETTINGS_SCOPE_ALL;
    return scope;
}

static inline bool rgb_settings_version_is_newer(uint32_t candidate, uint32_t reference)
{
    return (int32_t)(candidate - reference) > 0;
}

#ifdef __cplusplus
}
#endif

#endif // IO_RGB_SETTINGS_CONTRACT_V2_H
