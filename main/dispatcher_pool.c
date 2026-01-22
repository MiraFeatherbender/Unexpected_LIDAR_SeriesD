#include "dispatcher_pool.h"
#include "plugins/dispatcher_allocator.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <math.h>
#include <string.h>

#define POOL_MAX_NAME_LEN 16

typedef struct dispatcher_pool_s dispatcher_pool_t;

struct pool_msg_s {
    uint16_t ref;
    dispatcher_msg_ptr_t msg;
    dispatcher_pool_t *pool;
    struct pool_msg_s *next;
};

struct dispatcher_pool_s {
    char name[POOL_MAX_NAME_LEN];
    pool_msg_t *entries;
    pool_msg_t *free_list;
    size_t entry_count;
    size_t payload_size;
    uint8_t *payload_region;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t available;
    uint32_t alloc_failures;
    uint32_t in_use;
    uint32_t max_in_use;
};

static const char *TAG = "dispatcher_pool";

static dispatcher_pool_t streaming_pool = {0};
static dispatcher_pool_t control_pool = {0};

static int clamp_int(int value, int min_v, int max_v) {
    if (value < min_v) return min_v;
    if (value > max_v) return max_v;
    return value;
}

static int compute_entries(double F, int C, int min_e, int max_e) {
    double raw = ((double)TARGET_MAX) * F * (double)C;
    int entries = (int)ceil(raw);
    if (entries < 1) entries = 1;
    return clamp_int(entries, min_e, max_e);
}

static pool_msg_t *pool_pop(dispatcher_pool_t *pool) {
    if (!pool || !pool->free_list) return NULL;
    pool_msg_t *msg = pool->free_list;
    pool->free_list = msg->next;
    msg->next = NULL;
    return msg;
}

static void pool_push(dispatcher_pool_t *pool, pool_msg_t *msg) {
    if (!pool || !msg) return;
    msg->next = pool->free_list;
    pool->free_list = msg;
}

static void pool_log_config(const dispatcher_pool_t *pool) {
    if (!pool) return;
    ESP_LOGI(TAG, "%s pool: entries=%u payload=%u entry_size=%u",
             pool->name,
             (unsigned)pool->entry_count,
             (unsigned)pool->payload_size,
             (unsigned)(sizeof(pool_msg_t) + pool->payload_size));
}

static int pool_init(dispatcher_pool_t *pool, const char *name, const pool_config_t *cfg) {
    if (!pool || !name || !cfg) return -1;

    memset(pool, 0, sizeof(*pool));
    strncpy(pool->name, name, POOL_MAX_NAME_LEN - 1);
    pool->name[POOL_MAX_NAME_LEN - 1] = '\0';

    int entries = compute_entries(cfg->F, cfg->C, cfg->min_entries, cfg->max_entries);
    size_t payload_size = (size_t)cfg->payload_size;
    if (payload_size > BUF_SIZE) {
        ESP_LOGW(TAG, "%s pool payload_size %u > BUF_SIZE %u; capping",
                 pool->name, (unsigned)payload_size, (unsigned)BUF_SIZE);
        payload_size = BUF_SIZE;
    }

    pool->entries = (pool_msg_t *)heap_caps_calloc(entries, sizeof(pool_msg_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pool->entries) {
        ESP_LOGW(TAG, "%s pool PSRAM alloc failed; trying internal heap", pool->name);
        pool->entries = (pool_msg_t *)heap_caps_calloc(entries, sizeof(pool_msg_t), MALLOC_CAP_8BIT);
    }
    if (!pool->entries) {
        ESP_LOGE(TAG, "%s pool allocation failed (entries=%d)", pool->name, entries);
        return -2;
    }

    pool->payload_region = (uint8_t *)heap_caps_calloc(entries, payload_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pool->payload_region) {
        ESP_LOGW(TAG, "%s pool payload PSRAM alloc failed; trying internal heap", pool->name);
        pool->payload_region = (uint8_t *)heap_caps_calloc(entries, payload_size, MALLOC_CAP_8BIT);
    }
    if (!pool->payload_region) {
        ESP_LOGE(TAG, "%s pool payload allocation failed (entries=%d payload=%u)",
                 pool->name, entries, (unsigned)payload_size);
        return -2;
    }

    pool->entry_count = (size_t)entries;
    pool->payload_size = payload_size;

    pool->mutex = xSemaphoreCreateMutex();
    pool->available = xSemaphoreCreateCounting(entries, entries);

    if (!pool->mutex || !pool->available) {
        ESP_LOGE(TAG, "%s pool semaphore creation failed", pool->name);
        return -3;
    }

    for (int i = 0; i < entries; ++i) {
        pool->entries[i].ref = 0;
        pool->entries[i].pool = pool;
        pool->entries[i].msg.data = pool->payload_region + (i * payload_size);
        pool->entries[i].msg.message_len = 0;
        memset(pool->entries[i].msg.targets, TARGET_MAX, sizeof(pool->entries[i].msg.targets));
        pool->entries[i].next = pool->free_list;
        pool->free_list = &pool->entries[i];
    }

    pool_log_config(pool);
    return 0;
}

int dispatcher_pool_init(void) {
    const pool_config_t *streaming_cfg = dispatcher_allocator_get_streaming_config();
    const pool_config_t *control_cfg = dispatcher_allocator_get_control_config();
    if (!streaming_cfg || !control_cfg) {
        ESP_LOGE(TAG, "pool config not available");
        return -1;
    }

    int r1 = pool_init(&streaming_pool, "streaming", streaming_cfg);
    int r2 = pool_init(&control_pool, "control", control_cfg);

    if (r1 != 0 || r2 != 0) {
        ESP_LOGE(TAG, "dispatcher_pool_init failed (streaming=%d control=%d)", r1, r2);
        return -2;
    }

    dispatcher_pool_self_test();
    return 0;
}

static dispatcher_pool_t *pool_by_type(dispatcher_pool_type_t type) {
    return (type == DISPATCHER_POOL_CONTROL) ? &control_pool : &streaming_pool;
}

pool_msg_t *dispatcher_pool_try_alloc(dispatcher_pool_type_t type) {
    dispatcher_pool_t *pool = pool_by_type(type);
    if (!pool || !pool->available) return NULL;

    if (xSemaphoreTake(pool->available, 0) != pdTRUE) {
        pool->alloc_failures++;
        ESP_LOGW(TAG, "%s pool exhausted (failures=%u)", pool->name, pool->alloc_failures);
        return NULL;
    }

    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "%s pool mutex take failed", pool->name);
        xSemaphoreGive(pool->available);
        return NULL;
    }

    pool_msg_t *msg = pool_pop(pool);
    if (msg) {
        pool->in_use++;
        if (pool->in_use > pool->max_in_use) pool->max_in_use = pool->in_use;
    }

    xSemaphoreGive(pool->mutex);

    if (!msg) {
        ESP_LOGE(TAG, "%s pool internal empty despite semaphore", pool->name);
        xSemaphoreGive(pool->available);
        return NULL;
    }

    msg->ref = 1;
    uint8_t *payload = msg->msg.data;
    memset(&msg->msg, 0, sizeof(msg->msg));
    msg->msg.data = payload;
    msg->msg.message_len = 0;
    memset(msg->msg.targets, TARGET_MAX, sizeof(msg->msg.targets));
    return msg;
}

pool_msg_t *dispatcher_pool_alloc_blocking(dispatcher_pool_type_t type, uint32_t timeout_ms) {
    dispatcher_pool_t *pool = pool_by_type(type);
    if (!pool || !pool->available) return NULL;

    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(pool->available, ticks) != pdTRUE) {
        pool->alloc_failures++;
        ESP_LOGW(TAG, "%s pool alloc timed out (failures=%u)", pool->name, pool->alloc_failures);
        return NULL;
    }

    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "%s pool mutex take failed", pool->name);
        xSemaphoreGive(pool->available);
        return NULL;
    }

    pool_msg_t *msg = pool_pop(pool);
    if (msg) {
        pool->in_use++;
        if (pool->in_use > pool->max_in_use) pool->max_in_use = pool->in_use;
    }

    xSemaphoreGive(pool->mutex);

    if (!msg) {
        ESP_LOGE(TAG, "%s pool internal empty despite semaphore", pool->name);
        xSemaphoreGive(pool->available);
        return NULL;
    }

    msg->ref = 1;
    uint8_t *payload = msg->msg.data;
    memset(&msg->msg, 0, sizeof(msg->msg));
    msg->msg.data = payload;
    msg->msg.message_len = 0;
    memset(msg->msg.targets, TARGET_MAX, sizeof(msg->msg.targets));
    return msg;
}

void dispatcher_pool_msg_ref(pool_msg_t *msg) {
    if (!msg) return;
    __atomic_add_fetch(&msg->ref, 1, __ATOMIC_SEQ_CST);
}

void dispatcher_pool_msg_unref(pool_msg_t *msg) {
    if (!msg) return;
    uint16_t v = __atomic_sub_fetch(&msg->ref, 1, __ATOMIC_SEQ_CST);
    if (v > 0) return;

    dispatcher_pool_t *pool = msg->pool;
    if (!pool || !pool->mutex || !pool->available) return;

    if (xSemaphoreTake(pool->mutex, portMAX_DELAY) == pdTRUE) {
        pool_push(pool, msg);
        if (pool->in_use > 0) pool->in_use--;
        xSemaphoreGive(pool->mutex);
        xSemaphoreGive(pool->available);
    }
}

dispatcher_msg_ptr_t *dispatcher_pool_get_msg(pool_msg_t *msg) {
    return msg ? &msg->msg : NULL;
}

const dispatcher_msg_ptr_t *dispatcher_pool_get_msg_const(const pool_msg_t *msg) {
    return msg ? &msg->msg : NULL;
}

void dispatcher_pool_log_stats(void) {
    dispatcher_pool_t *pools[2] = { &streaming_pool, &control_pool };
    for (int i = 0; i < 2; ++i) {
        dispatcher_pool_t *p = pools[i];
        if (!p || !p->entries) continue;
        ESP_LOGI(TAG, "%s pool stats: total=%u in_use=%u max_in_use=%u alloc_failures=%u",
                 p->name,
                 (unsigned)p->entry_count,
                 (unsigned)p->in_use,
                 (unsigned)p->max_in_use,
                 (unsigned)p->alloc_failures);
    }
}

void dispatcher_pool_self_test(void) {
    ESP_LOGI(TAG, "dispatcher_pool self-test begin");

    pool_msg_t *a = dispatcher_pool_try_alloc(DISPATCHER_POOL_STREAMING);
    if (a) {
        dispatcher_msg_ptr_t *msg = dispatcher_pool_get_msg(a);
        if (msg && msg->data) {
            const uint8_t pattern = 0xA5;
            size_t len = streaming_pool.payload_size > 16 ? 16 : streaming_pool.payload_size;
            memset(msg->data, pattern, len);
            bool ok = true;
            for (size_t i = 0; i < len; ++i) {
                if (msg->data[i] != pattern) {
                    ok = false;
                    break;
                }
            }
            ESP_LOGI(TAG, "streaming alloc OK (payload %s)", ok ? "OK" : "FAIL");
        } else {
            ESP_LOGE(TAG, "streaming alloc OK but payload pointer NULL");
        }
        dispatcher_pool_msg_unref(a);
    } else {
        ESP_LOGE(TAG, "streaming alloc FAILED");
    }

    pool_msg_t *b = dispatcher_pool_alloc_blocking(DISPATCHER_POOL_CONTROL, 50);
    if (b) {
        dispatcher_msg_ptr_t *msg = dispatcher_pool_get_msg(b);
        if (msg && msg->data) {
            const uint8_t pattern = 0x5A;
            size_t len = control_pool.payload_size > 16 ? 16 : control_pool.payload_size;
            memset(msg->data, pattern, len);
            bool ok = true;
            for (size_t i = 0; i < len; ++i) {
                if (msg->data[i] != pattern) {
                    ok = false;
                    break;
                }
            }
            ESP_LOGI(TAG, "control alloc OK (payload %s)", ok ? "OK" : "FAIL");
        } else {
            ESP_LOGE(TAG, "control alloc OK but payload pointer NULL");
        }
        dispatcher_pool_msg_unref(b);
    } else {
        ESP_LOGE(TAG, "control alloc FAILED");
    }

    dispatcher_pool_log_stats();

    QueueHandle_t test_queue = xQueueCreate(1, sizeof(pool_msg_t *));
    if (test_queue) {
        dispatcher_register_ptr_queue(TARGET_LOG, test_queue);
        pool_msg_t *p = dispatcher_pool_try_alloc(DISPATCHER_POOL_STREAMING);
        if (p) {
            dispatcher_msg_ptr_t *pm = dispatcher_pool_get_msg(p);
            if (pm && pm->data) {
                const uint8_t pattern = 0xCC;
                size_t len = streaming_pool.payload_size > 8 ? 8 : streaming_pool.payload_size;
                memset(pm->data, pattern, len);
                pm->message_len = len;
                memset(pm->targets, TARGET_MAX, sizeof(pm->targets));
                pm->targets[0] = TARGET_LOG;

                int sent = dispatcher_broadcast_ptr(p, pm->targets);
                if (sent == 1) {
                    pool_msg_t *rx = NULL;
                    if (xQueueReceive(test_queue, &rx, pdMS_TO_TICKS(50)) == pdTRUE && rx == p) {
                        const dispatcher_msg_ptr_t *rxm = dispatcher_pool_get_msg_const(rx);
                        bool ok = true;
                        for (size_t i = 0; i < len; ++i) {
                            if (!rxm || !rxm->data || rxm->data[i] != pattern) {
                                ok = false;
                                break;
                            }
                        }
                        ESP_LOGI(TAG, "pointer broadcast %s", ok ? "OK" : "FAIL");
                        dispatcher_pool_msg_unref(rx);
                    } else {
                        ESP_LOGE(TAG, "pointer broadcast receive FAILED");
                    }
                } else {
                    ESP_LOGE(TAG, "pointer broadcast send FAILED (sent=%d)", sent);
                }
            }
        }
        vQueueDelete(test_queue);
        dispatcher_register_ptr_queue(TARGET_LOG, NULL);
    } else {
        ESP_LOGE(TAG, "pointer broadcast test queue create FAILED");
    }

    ESP_LOGI(TAG, "dispatcher_pool self-test end");
}

size_t dispatcher_pool_payload_size(dispatcher_pool_type_t type) {
    return (type == DISPATCHER_POOL_CONTROL) ? control_pool.payload_size : streaming_pool.payload_size;
}

pool_msg_t *dispatcher_pool_send_ptr(dispatcher_pool_type_t type,
                                     dispatch_source_t source,
                                     const dispatch_target_t *targets,
                                     const uint8_t *data,
                                     size_t data_len,
                                     void *context) {
    pool_msg_t *pmsg = dispatcher_pool_try_alloc(type);
    if (!pmsg) {
        ESP_LOGW(TAG, "pool alloc failed for source %d", source);
        return NULL;
    }

    dispatcher_msg_ptr_t *msg = dispatcher_pool_get_msg(pmsg);
    if (!msg || !msg->data) {
        ESP_LOGW(TAG, "pool message missing payload for source %d", source);
        dispatcher_pool_msg_unref(pmsg);
        return NULL;
    }

    msg->source = source;
    msg->context = context;
    if (targets) {
        memcpy(msg->targets, targets, sizeof(msg->targets));
    } else {
        memset(msg->targets, TARGET_MAX, sizeof(msg->targets));
    }

    size_t copy_len = data_len;
    if (copy_len > 0 && msg->data && data) {
        size_t max_len = (type == DISPATCHER_POOL_CONTROL) ? control_pool.payload_size : streaming_pool.payload_size;
        if (copy_len > max_len) copy_len = max_len;
        memcpy(msg->data, data, copy_len);
    }
    msg->message_len = copy_len;

    dispatcher_broadcast_ptr(pmsg, msg->targets);
    return pmsg;
}
