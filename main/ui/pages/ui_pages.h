#pragma once

#include "ui/pages/ui_page.h"

// Auto-generate page id enums from pages.def
#define X_PAGE(name) UI_PAGE_##name,
typedef enum {
#include "pages.def"
    UI_PAGE_COUNT
} ui_page_id_t;
#undef X_PAGE

// Optional: generate human-readable names array
#define X_PAGE(name) #name,
static const char * const ui_page_names[] = {
#include "pages.def"
};
#undef X_PAGE
