#ifndef DISPATCHER_POOL_H
#define DISPATCHER_POOL_H

#include <stdint.h>
#include "dispatcher.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pool_msg_s pool_msg_t;

typedef struct {
    dispatch_source_t source;
    dispatch_target_t targets[TARGET_MAX];
    size_t message_len;
    uint8_t *data;
    void *context;
} dispatcher_msg_ptr_t;

typedef enum {
    DISPATCHER_POOL_STREAMING = 0,
    DISPATCHER_POOL_CONTROL = 1
} dispatcher_pool_type_t;

typedef struct {
    dispatcher_pool_type_t type;
    dispatch_source_t source;
    const dispatch_target_t *targets;
    const uint8_t *data;
    size_t data_len;
    void *context;
} dispatcher_pool_send_params_t;

int dispatcher_pool_init(void);

pool_msg_t *dispatcher_pool_try_alloc(dispatcher_pool_type_t type);
pool_msg_t *dispatcher_pool_alloc_blocking(dispatcher_pool_type_t type, uint32_t timeout_ms);

void dispatcher_pool_msg_ref(pool_msg_t *msg);
void dispatcher_pool_msg_unref(pool_msg_t *msg);

dispatcher_msg_ptr_t *dispatcher_pool_get_msg(pool_msg_t *msg);
const dispatcher_msg_ptr_t *dispatcher_pool_get_msg_const(const pool_msg_t *msg);

void dispatcher_pool_log_stats(void);
void dispatcher_pool_self_test(void);
size_t dispatcher_pool_payload_size(dispatcher_pool_type_t type);

pool_msg_t *dispatcher_pool_send_ptr(dispatcher_pool_type_t type,
                                     dispatch_source_t source,
                                     const dispatch_target_t *targets,
                                     const uint8_t *data,
                                     size_t data_len,
                                     void *context);

pool_msg_t *dispatcher_pool_send_ptr_params(const dispatcher_pool_send_params_t *params);

#ifdef __cplusplus
}
#endif

#endif // DISPATCHER_POOL_H
