#include "mod_line_sensor_window.h"
#include "dispatcher_module.h"

static void line_sensor_window_on_msg(const dispatcher_msg_t *msg) {
    (void)msg;
    // Process message
}

static dispatcher_module_t line_sensor_window_mod = {
    .name = "line_sensor_window_task",
    .target = TARGET_LINE_SENSOR_WINDOW,
    .queue_len = 16,
    .stack_size = 4096,
    .task_prio = 9,
    .on_msg = line_sensor_window_on_msg,
    .queue = NULL
};

static void line_sensor_window_dispatcher_handler(const dispatcher_msg_t *msg) {
    dispatcher_module_enqueue(&line_sensor_window_mod, msg);
}

void mod_line_sensor_window_init(void) {
    dispatcher_module_start(&line_sensor_window_mod, line_sensor_window_dispatcher_handler);
}
