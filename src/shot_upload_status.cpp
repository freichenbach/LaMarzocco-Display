#include "shot_upload_status.h"
#include "config.h"
#include "ui/ui.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t gui_mutex;

namespace {

lv_obj_t *g_label = nullptr;
uint32_t g_hide_at_ms = 0;

// Lazily created, like update_screen.cpp's Bluetooth marker - keeps this
// file independent of screen init order.
void ensure_created()
{
    if (g_label || !ui_mainScreen) {
        return;
    }
    g_label = lv_label_create(ui_mainScreen);
    lv_obj_set_style_text_font(g_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(g_label, "");
    lv_obj_set_align(g_label, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(g_label, -6);
}

}  // namespace

void shot_upload_status_show(bool success, const char *message)
{
    if (!gui_mutex || xSemaphoreTake(gui_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    ensure_created();
    if (g_label) {
        lv_obj_set_style_text_color(g_label,
                                    lv_color_hex(success ? SHOT_UPLOAD_OK_COLOR : SHOT_UPLOAD_ERROR_COLOR),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(g_label, message);
    }
    xSemaphoreGive(gui_mutex);

    g_hide_at_ms = millis() + SHOT_UPLOAD_STATUS_MS;
}

void shot_upload_status_loop(void)
{
    if (!g_label || g_hide_at_ms == 0 || millis() < g_hide_at_ms) {
        return;
    }
    g_hide_at_ms = 0;

    if (!gui_mutex || xSemaphoreTake(gui_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    lv_label_set_text(g_label, "");
    xSemaphoreGive(gui_mutex);
}
