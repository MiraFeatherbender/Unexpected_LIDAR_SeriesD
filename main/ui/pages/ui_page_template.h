#pragma once

#include "ui/pages/ui_page.h"

// Template page id (pick an unused id when copying)

// Page API
esp_err_t ui_page_template_init(void);
void ui_page_template_deinit(void);
void ui_page_template_show(lv_obj_t *parent);
void ui_page_template_hide(void);

// Export page descriptor for registration
// Export page descriptor for registration (symbol name matches pages.def X_PAGE entries)
extern const ui_page_t ui_page_TEMPLATE;
