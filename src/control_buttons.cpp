#include "control_buttons.h"
#include "config.h"
#include "lamarzocco_machine.h"
#include "ui/ui.h"
#include <Arduino.h>

extern LaMarzoccoMachine* g_machine;

extern "C" const lv_img_dsc_t control_icon_power;
extern "C" const lv_img_dsc_t control_icon_steam;

// Both buttons toggle, so what matters is the state they toggle from. They
// draw that state like a light switch: a filled disc when on, an outline when
// off. The styles are set here rather than in the generated screen code, so a
// re-export from SquareLine Studio does not drop them.

#define CONTROL_BUTTON_SIZE      68
#define CONTROL_BUTTON_BORDER    2
#define CONTROL_BUTTON_POLL_MS   200

typedef struct {
    lv_obj_t* obj;
    bool pending;
    bool pending_from;         // State when tapped; a change ends the wait
    uint32_t pending_since;
    int shown;                 // Last drawn look, -1 = not drawn yet
} ControlButtonInfo;

static ControlButtonInfo g_buttons[CONTROL_BUTTON_COUNT];
static lv_timer_t* g_timer = nullptr;

static bool current_state(ControlButton button)
{
    if (!g_machine) {
        return false;
    }
    return (button == CONTROL_BUTTON_POWER) ? g_machine->get_power_state()
                                            : g_machine->get_steam_state();
}

static void apply_look(ControlButtonInfo* b, bool on, bool pending)
{
    int look = (on ? 1 : 0) | (pending ? 2 : 0);
    if (b->shown == look) {
        return;
    }
    b->shown = look;

    lv_obj_t* obj = b->obj;
    if (on) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(CONTROL_BUTTON_ON_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(CONTROL_BUTTON_ON_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj, lv_color_hex(CONTROL_BUTTON_ON_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_recolor(obj, lv_color_hex(CONTROL_BUTTON_ON_ICON), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj, lv_color_hex(CONTROL_BUTTON_OFF_BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_recolor(obj, lv_color_hex(CONTROL_BUTTON_OFF_ICON), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_bg_img_opa(obj, pending ? CONTROL_BUTTON_PENDING_OPA : LV_OPA_COVER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_invalidate(obj);
}

// Runs in the LVGL task, which holds the GUI mutex.
static void refresh(lv_timer_t* timer)
{
    (void)timer;
    uint32_t now = millis();
    for (int i = 0; i < CONTROL_BUTTON_COUNT; i++) {
        ControlButtonInfo* b = &g_buttons[i];
        if (!b->obj) {
            continue;
        }
        bool on = current_state((ControlButton)i);
        // The wait ends when the machine reports the other state, or after a
        // while if it never does (command failed or lost): then the button
        // simply shows the last known state again.
        if (b->pending && (on != b->pending_from ||
                           now - b->pending_since > CONTROL_BUTTON_PENDING_MS)) {
            b->pending = false;
        }
        apply_look(b, on, b->pending);
    }
}

static void setup_button(ControlButtonInfo* b, lv_obj_t* obj, const lv_img_dsc_t* icon)
{
    b->obj = obj;
    b->pending = false;
    b->shown = -1;
    if (!obj) {
        return;
    }

    lv_obj_set_size(obj, CONTROL_BUTTON_SIZE, CONTROL_BUTTON_SIZE);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, CONTROL_BUTTON_BORDER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(obj, icon, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void control_buttons_init(void)
{
    setup_button(&g_buttons[CONTROL_BUTTON_POWER], ui_powerButton, &control_icon_power);
    setup_button(&g_buttons[CONTROL_BUTTON_STEAM], ui_steamButton, &control_icon_steam);
    refresh(nullptr);
    if (!g_timer) {
        g_timer = lv_timer_create(refresh, CONTROL_BUTTON_POLL_MS, nullptr);
    }
}

void control_buttons_mark_pending(ControlButton button)
{
    if (button >= CONTROL_BUTTON_COUNT) {
        return;
    }
    ControlButtonInfo* b = &g_buttons[button];
    b->pending = true;
    b->pending_from = current_state(button);
    b->pending_since = millis();
    apply_look(b, b->pending_from, true);
}
