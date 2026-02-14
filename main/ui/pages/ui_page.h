#pragma once

#include "esp_err.h"
#include "lvgl.h"

typedef struct ui_page_s {
    int id;                  // numeric page id
    const char *name;        // human readable name
    esp_err_t (*init)(lv_obj_t *parent); // allocate resources, parent is page tile
    void (*deinit)(void);    // free resources
    void (*show)(lv_obj_t *parent); // create/attach widgets under parent or start page
    void (*hide)(void);      // hide/stop timers
} ui_page_t;
