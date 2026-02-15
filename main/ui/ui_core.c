#include "ui_core.h"
#include "ui/pages/ui_page.h"
#include "ui/pages/ui_pages.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "ui_input_adapter.h"
#include "ui_styles.h"

static const char *TAG = "ui_core";

#define MAX_PAGES 16
// Declare page descriptors generated from pages.def
#define X_PAGE(name) extern const ui_page_t ui_page_##name;
#include "pages/pages.def"
#undef X_PAGE

// Build static page pointer array from pages.def
#define X_PAGE(name) &ui_page_##name,
static const ui_page_t *s_pages[] = {
#include "pages/pages.def"
};
#undef X_PAGE

static int s_active_index = -1;
// Tileview UI: one tile per page arranged horizontally
static lv_obj_t *s_tileview = NULL;
static lv_obj_t *s_tile_pages[UI_PAGE_COUNT];
// Title overlay shown above the tileview
static lv_obj_t *s_title_overlay = NULL;
// Status overlay (e.g., battery symbol) drawn above the title
static lv_obj_t *s_status_overlay = NULL;

// Page callback used by the input adapter. Defined at file scope to avoid
// nested-function issues on C compilers.
static void ui_core_page_cb(int8_t dir) { if (dir > 0) ui_core_show_next(); else ui_core_show_prev(); }

static int find_page_index_by_id(int page_id)
{
    for (int i = 0; i < UI_PAGE_COUNT; ++i) {
        if (s_pages[i] && s_pages[i]->id == page_id) return i;
    }
    return -1;
}

esp_err_t ui_core_show_page(int page_id)
{
    int idx = find_page_index_by_id(page_id);
    if (idx < 0) return ESP_ERR_NOT_FOUND;

    // If we have a tileview, activate the corresponding tile (columns = page index)
    if (s_tileview) {
        lvgl_port_lock(0);
        lv_tileview_set_tile_by_index(s_tileview, idx, 0, LV_ANIM_ON);
        // update title overlay to current page name
        if (s_title_overlay && s_pages[idx] && s_pages[idx]->name) {
            // lv_label_set_text(s_title_overlay, s_pages[idx]->name);
            lv_label_set_text_fmt(s_title_overlay, "%s %s %s", "", s_pages[idx]->name, "");
        }
        /* Invoke per-activation hook so the page can start timers/animations.
         * show() is used as the activation entry point to keep a single
         * callback for pages (existing pages already implement show()). */
        if (s_pages[idx] && s_pages[idx]->show && s_tile_pages[idx]) {
            s_pages[idx]->show(s_tile_pages[idx]);
        }
        lvgl_port_unlock();
        s_active_index = idx;
        // ESP_LOGI(TAG, "show page (tile) id=%d index=%d", page_id, idx);
        return ESP_OK;
    }

    // // Fallback: call init only (do not show) and update index
    // if (s_pages[idx] && s_pages[idx]->init) {
    //     if (s_pages[idx]->init() != ESP_OK) {
    //         ESP_LOGW(TAG, "page init failed id=%d", s_pages[idx]->id);
    //     }
    // }
    s_active_index = idx;
    // ESP_LOGI(TAG, "show page id=%d index=%d", page_id, idx);
    return ESP_OK;
}

void ui_core_show_next(void)
{
    if (UI_PAGE_COUNT == 0) return;
    int next = (s_active_index + 1) % UI_PAGE_COUNT;
    ui_core_show_page(s_pages[next]->id);
}

void ui_core_show_prev(void)
{
    if (UI_PAGE_COUNT == 0) return;
    int prev = (s_active_index - 1 + UI_PAGE_COUNT) % UI_PAGE_COUNT;
    ui_core_show_page(s_pages[prev]->id);
}

esp_err_t ui_core_init(void)
{
    // nothing heavy here; pages are auto-registered via pages.def
    ESP_LOGI(TAG, "ui_core initialized (pages=%d)", UI_PAGE_COUNT);
    // Register the page-callback so press+rotate navigates pages.
    ui_input_set_page_callback(ui_core_page_cb);

    // No-op widget callback by default; LVGL will handle rotate-only and presses
    ui_input_set_widget_callback(NULL);
    // Initialize shared styles
    ui_styles_init();
    
    // Create a tileview and a tile for each registered page. Each page's
    // `show` will be called with its tile container as parent so pages
    // populate their tile content.
    lvgl_port_lock(0);
    {
        lv_obj_t *scr = lv_scr_act();
        lv_coord_t sw = lv_obj_get_width(scr);
        lv_coord_t sh = lv_obj_get_height(scr);

        s_tileview = lv_tileview_create(scr);
        if (s_tileview) {
            lv_obj_set_size(s_tileview, sw, sh);
            lv_obj_add_style(s_tileview, &ui_style_dark_mode, 0);
            // create one column per page in row 0
            for (int i = 0; i < UI_PAGE_COUNT; ++i) {
                const ui_page_t *p = s_pages[i];
                lv_obj_t *tile = lv_tileview_add_tile(s_tileview, i, 0, LV_DIR_HOR);
                s_tile_pages[i] = tile;
                if (tile) {
                    lv_obj_add_style(tile, &ui_style_dark_mode, 0);
                }
                if (p) {
                    if (p->init) {
                        if (p->init(tile) != ESP_OK) {
                            ESP_LOGW(TAG, "page init failed id=%d", p->id);
                        }
                    }
                    /* Do not call p->show() here; show() will be invoked when the
                     * page becomes active to separate one-time creation from
                     * per-activation behavior. */
                }
            }

            // activate first tile and call its show() (per-activation hook)
            lv_tileview_set_tile_by_index(s_tileview, 0, 0, LV_ANIM_OFF);
            if (UI_PAGE_COUNT > 0 && s_pages[0] && s_pages[0]->show && s_tile_pages[0]) {
                s_pages[0]->show(s_tile_pages[0]);
            }
            s_active_index = 0;
            // create title overlay above tiles (use dark-mode style)
            s_title_overlay = lv_label_create(scr);
            if (s_title_overlay) {
                const char *t = (UI_PAGE_COUNT > 0 && s_pages[0] && s_pages[0]->name) ? s_pages[0]->name : "";
                lv_obj_add_style(s_title_overlay, &ui_style_dark_mode, 0);
                lv_obj_set_size(s_title_overlay, 128, 16);
                lv_obj_align(s_title_overlay, LV_ALIGN_TOP_MID, 0, -2);
                lv_label_set_text_fmt(s_title_overlay, "%s %s %s", "", t, "");
                lv_obj_set_style_text_align(s_title_overlay, LV_TEXT_ALIGN_LEFT, 0);
                lv_obj_set_style_text_font(s_title_overlay, &lv_font_montserrat_12, 0);

                // Create a status overlay label above the title (transparent bg)
                s_status_overlay = lv_label_create(scr);
                if (s_status_overlay) {
                    lv_obj_add_style(s_status_overlay, &ui_style_dark_mode, 0);
                    lv_obj_set_size(s_status_overlay, 128, 16);
                    lv_obj_align(s_status_overlay, LV_ALIGN_TOP_MID, 0, -2);
                    lv_obj_set_style_bg_opa(s_status_overlay, LV_OPA_TRANSP, 0);
                    lv_label_set_text(s_status_overlay, LV_SYMBOL_BATTERY_FULL);
                    lv_obj_set_style_text_font(s_status_overlay, &lv_font_montserrat_14, 0);
                    lv_obj_set_style_text_align(s_status_overlay, LV_TEXT_ALIGN_RIGHT, 0);
                }
            }
        } else {
            ESP_LOGW(TAG, "lv_tileview_create failed; falling back to screen mode");
        }
    }
    lvgl_port_unlock();

    return ESP_OK;
}

void ui_core_deinit(void)
{
    // call deinit on pages
    for (int i = 0; i < UI_PAGE_COUNT; ++i) {
        if (s_pages[i] && s_pages[i]->deinit) s_pages[i]->deinit();
    }
    lvgl_port_lock(0);
    if (s_tileview) {
        lv_obj_del(s_tileview);
        s_tileview = NULL;
    }
    if (s_title_overlay) {
        lv_obj_del(s_title_overlay);
        s_title_overlay = NULL;
    }
    lvgl_port_unlock();
    s_active_index = -1;
}
