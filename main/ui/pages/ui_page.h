#pragma once

#include "esp_err.h"
#include "lvgl.h"

typedef struct ui_page_s {
    int id;                  // numeric page id
    const char *name;        // human readable name
    esp_err_t (*init)(void); // allocate resources
    void (*deinit)(void);    // free resources
    void (*show)(lv_obj_t *parent); // create/attach widgets under parent
    void (*hide)(void);      // hide/stop timers
} ui_page_t;
