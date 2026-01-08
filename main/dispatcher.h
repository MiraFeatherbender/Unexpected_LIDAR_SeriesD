#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

#define BUF_SIZE (1024)

// enums and structs for communication state and context

typedef enum {
    SOURCE_USB_CDC,
    SOURCE_USB_MSC,
    SOURCE_UART,
    SOURCE_LIDAR_IO,
    SOURCE_LIDAR_COORD,
    SOURCE_MSC_BUTTON,
    SOURCE_LINE_SENSOR,
    SOURCE_UNDEFINED
} dispatch_source_t;

typedef enum {
    TARGET_USB_CDC,
    TARGET_USB_MSC,
    TARGET_UART,
    TARGET_RGB,
    TARGET_LIDAR_IO,
    TARGET_LIDAR_COORD,
    TARGET_MAX
} dispatch_target_t;

typedef struct {
    dispatch_source_t source;
    dispatch_target_t targets[TARGET_MAX];
    size_t message_len;
    uint8_t data[BUF_SIZE];
} dispatcher_msg_t;

typedef void (*dispatcher_handler_t)(const dispatcher_msg_t *msg);

void dispatcher_init(void);
void dispatcher_send(const dispatcher_msg_t *msg);
void dispatcher_send_from_isr(const dispatcher_msg_t *msg,
                              BaseType_t *hp_task_woken);
void dispatcher_register_handler(dispatch_target_t target,
                                 dispatcher_handler_t handler);

#endif // DISPATCHER_H