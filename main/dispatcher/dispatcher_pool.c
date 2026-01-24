#include "dispatcher_pool.h"
#include "dispatcher_allocator.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
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
    uint8_t on_free_list; /* flag: 1 if currently on pool free_list */
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
    uint32_t double_free_count; /* number of detected double-unrefs */
    uint32_t corrupt_checks;    /* number of consistency checks performed */
};

static const char *TAG = "dispatcher_pool";

static dispatcher_pool_t streaming_pool = {0};
static dispatcher_pool_t control_pool = {0};

// Forward declare dispatcher_pool_stats_task
static void dispatcher_pool_stats_task(void *arg);

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
    msg->on_free_list = 0;
    return msg;
}

static void pool_push(dispatcher_pool_t *pool, pool_msg_t *msg) {
    if (!pool || !msg) return;
    msg->on_free_list = 1;
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
        dispatcher_fill_targets(pool->entries[i].msg.targets);
        pool->entries[i].on_free_list = 1; /* initially free */
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

    // Periodic pool stats task disabled temporarily (was every 10s).
#if 0
    BaseType_t ok = xTaskCreate(
        (TaskFunction_t)dispatcher_pool_stats_task,
        "dispatcher_pool_stats",
        4096,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
    if (ok != pdPASS) {
        ESP_LOGW(TAG, "Failed to create dispatcher_pool_stats task");
    }
#endif

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
        ESP_LOGE(TAG, "%s pool internal empty despite semaphore (in_use=%u entry_count=%u)", pool->name, (unsigned)pool->in_use, (unsigned)pool->entry_count);
        /* Consistency check */
        if (xSemaphoreTake(pool->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            size_t free_count = 0;
            pool_msg_t *it = pool->free_list;
            while (it) { free_count++; it = it->next; }
            ESP_LOGW(TAG, "%s pool consistency: free_count=%u in_use=%u entry_count=%u double_free_count=%u",
                     pool->name, (unsigned)free_count, (unsigned)pool->in_use, (unsigned)pool->entry_count, (unsigned)pool->double_free_count);
            if (pool->entry_count <= 64) {
                for (size_t i = 0; i < pool->entry_count; ++i) {
                    pool_msg_t *e = &pool->entries[i];
                    ESP_LOGW(TAG, " entry[%u] ref=%u on_free_list=%u", (unsigned)i, (unsigned)e->ref, (unsigned)e->on_free_list);
                }
            }
            pool->corrupt_checks++;
            xSemaphoreGive(pool->mutex);
        }
        xSemaphoreGive(pool->available);
        return NULL;
    }

    msg->ref = 1;
    uint8_t *payload = msg->msg.data;
    memset(&msg->msg, 0, sizeof(msg->msg));
    msg->msg.data = payload;
    msg->msg.message_len = 0;
    dispatcher_fill_targets(msg->msg.targets);
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
        ESP_LOGE(TAG, "%s pool internal empty despite semaphore (in_use=%u entry_count=%u)", pool->name, (unsigned)pool->in_use, (unsigned)pool->entry_count);
        /* Consistency check */
        if (xSemaphoreTake(pool->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            size_t free_count = 0;
            pool_msg_t *it = pool->free_list;
            while (it) { free_count++; it = it->next; }
            ESP_LOGW(TAG, "%s pool consistency: free_count=%u in_use=%u entry_count=%u double_free_count=%u",
                     pool->name, (unsigned)free_count, (unsigned)pool->in_use, (unsigned)pool->entry_count, (unsigned)pool->double_free_count);
            if (pool->entry_count <= 64) {
                for (size_t i = 0; i < pool->entry_count; ++i) {
                    pool_msg_t *e = &pool->entries[i];
                    ESP_LOGW(TAG, " entry[%u] ref=%u on_free_list=%u", (unsigned)i, (unsigned)e->ref, (unsigned)e->on_free_list);
                }
            }
            pool->corrupt_checks++;
            xSemaphoreGive(pool->mutex);
        }
        xSemaphoreGive(pool->available);
        return NULL;
    }

    msg->ref = 1;
    uint8_t *payload = msg->msg.data;
    memset(&msg->msg, 0, sizeof(msg->msg));
    msg->msg.data = payload;
    msg->msg.message_len = 0;
    dispatcher_fill_targets(msg->msg.targets);
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
        // Detect double-unref / double-push: if msg already on free_list, skip push
        if (msg->on_free_list) {
            pool->double_free_count++;
            const char *task = pcTaskGetName(NULL);
            void *ra = __builtin_return_address(0);
            ESP_LOGW(TAG, "double-unref detected: msg=%p source=%d ref_after_unsub=%u (skipping push) task=%s ra=%p", (void*)msg, (int)msg->msg.source, (unsigned)v, task ? task : "(null)", ra);
            /* Optional: print small backtrace addresses for quicker triage */
            void *ra1 = __builtin_return_address(1);
            void *ra2 = __builtin_return_address(2);
            ESP_LOGW(TAG, "  backtrace addrs: ra0=%p ra1=%p ra2=%p", ra, ra1, ra2);
            // Sanity: ensure in_use is non-zero before decrement
            if (pool->in_use > 0) pool->in_use--;
            xSemaphoreGive(pool->mutex);
            return;
        }

        // Normal return to pool
        pool_push(pool, msg);
        if (pool->in_use > 0) pool->in_use--;
        xSemaphoreGive(pool->mutex);
        xSemaphoreGive(pool->available);
    }

    // Detect suspicious ref wrap underflow
    if (v > 1000 || (pool && v > pool->entry_count)) {
        ESP_LOGW(TAG, "suspicious ref value after unref: %u for msg=%p", (unsigned)v, (void*)msg);
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
        ESP_LOGI(TAG, "%s pool stats: total=%u in_use=%u max_in_use=%u alloc_failures=%u double_free=%u checks=%u",
                 p->name,
                 (unsigned)p->entry_count,
                 (unsigned)p->in_use,
                 (unsigned)p->max_in_use,
                 (unsigned)p->alloc_failures,
                 (unsigned)p->double_free_count,
                 (unsigned)p->corrupt_checks);
    }
}

static void dispatcher_pool_stats_task(void *arg) {
    (void)arg;
    for (;;) {
        dispatcher_pool_log_stats();
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "dispatcher_pool_stats stack high-water mark: %u", (unsigned)hwm);
        vTaskDelay(pdMS_TO_TICKS(10000));
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
                dispatcher_fill_targets(pm->targets);
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
    dispatcher_pool_send_params_t params = {
        .type = type,
        .source = source,
        .targets = targets,
        .data = data,
        .data_len = data_len,
        .context = context
    };
    return dispatcher_pool_send_ptr_params(&params);
}

pool_msg_t *dispatcher_pool_send_ptr_params(const dispatcher_pool_send_params_t *params) {
    if (!params) return NULL;

    pool_msg_t *pmsg = dispatcher_pool_try_alloc(params->type);
    if (!pmsg) {
        ESP_LOGW(TAG, "pool alloc failed for source %d", params->source);
        // Diagnostic: log per-target queue depth & capacity to help diagnose which consumers are backlogged
        if (params && params->targets) {
            for (int i = 0; i < TARGET_MAX; ++i) {
                dispatch_target_t t = params->targets[i];
                if (t == TARGET_MAX) continue;
                QueueHandle_t q = dispatcher_get_ptr_queue(t);
                if (!q) {
                    ESP_LOGW(TAG, " target %d: no pointer queue registered", (int)t);
                } else {
                    UBaseType_t waiting = uxQueueMessagesWaiting(q);
                    UBaseType_t spaces = uxQueueSpacesAvailable(q);
                    UBaseType_t capacity = waiting + spaces; // approximate queue length
                    ESP_LOGW(TAG, " target %d: queue depth %u/%u (waiting=%u spaces=%u)", (int)t, (unsigned)waiting, (unsigned)capacity, (unsigned)waiting, (unsigned)spaces);
                }
            }
        }
        return NULL;
    }

    dispatcher_msg_ptr_t *msg = dispatcher_pool_get_msg(pmsg);
    if (!msg || !msg->data) {
        ESP_LOGW(TAG, "pool message missing payload for source %d", params->source);
        dispatcher_pool_msg_unref(pmsg);
        return NULL;
    }

    msg->source = params->source;
    msg->context = params->context;
    if (params->targets) {
        memcpy(msg->targets, params->targets, sizeof(msg->targets));
    } else {
        dispatcher_fill_targets(msg->targets);
    }

    size_t copy_len = params->data_len;
    if (copy_len > 0 && msg->data && params->data) {
        size_t max_len = (params->type == DISPATCHER_POOL_CONTROL) ? control_pool.payload_size : streaming_pool.payload_size;
        if (copy_len > max_len) copy_len = max_len;
        memcpy(msg->data, params->data, copy_len);
    }
    msg->message_len = copy_len;

    dispatcher_broadcast_ptr(pmsg, msg->targets);
    return pmsg;
}
