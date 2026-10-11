#include "backflush_view.h"

#include "config.h"
#include "machine_actions.h"
#include "activity_monitor.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "ui/ui.h"

extern Preferences preferences;

namespace {

// What the machine says it is doing.
enum MachineBackflush : uint8_t {
    BF_OFF = 0,
    BF_REQUESTED,  // waiting for the paddle
    BF_CLEANING,
};

// What the view is showing.
enum ViewState : uint8_t {
    VIEW_HIDDEN = 0,
    VIEW_BLOCKED,   // cannot start: the machine is off
    VIEW_CONFIRM,   // blind filter warning
    VIEW_WAITING,   // command sent, the machine wants the paddle moved
    VIEW_RUNNING,
    VIEW_DONE,      // finished, or never started
};

// Written by the main loop, read by the LVGL timer. Single bytes, so a torn
// read is not possible and no lock is needed.
volatile MachineBackflush g_machine_bf = BF_OFF;
volatile bool g_machine_on = false;
// Whether a dashboard has arrived at all, and whether it carried the cleaning
// widget. Without the second one the machine does not know the command, and
// offering to start it would only produce a failure the user cannot act on.
volatile bool g_saw_dashboard = false;
volatile bool g_saw_widget = false;

ViewState g_state = VIEW_HIDDEN;
unsigned long g_state_since = 0;

lv_obj_t *g_root      = nullptr;
lv_obj_t *g_title     = nullptr;
lv_obj_t *g_headline  = nullptr;
lv_obj_t *g_detail    = nullptr;
lv_obj_t *g_start_btn = nullptr;
lv_obj_t *g_close_btn = nullptr;
lv_obj_t *g_close_lbl = nullptr;
lv_obj_t *g_arc       = nullptr;
lv_obj_t *g_spinner   = nullptr;
lv_timer_t *g_timer   = nullptr;

// The cloud reports no duration and no progress, only the three states. So the
// length of a cycle is measured the first time one runs and kept, and from the
// second cycle on it can be counted down. Until then there is nothing honest to
// count, and the spinner runs instead.
uint32_t g_known_duration_s = 0;
unsigned long g_caption_since = 0;
uint8_t g_caption_index = 0;

const char *const CAPTIONS[] = {
    "Sprudelt",
    "Schrubbt",
    "Blubbert",
    "Gurgelt",
    "Macht sauber",
};
const uint8_t CAPTION_COUNT = sizeof(CAPTIONS) / sizeof(CAPTIONS[0]);

void show(lv_obj_t *object, bool visible)
{
    if (!object) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

void show_progress(bool visible, bool determinate)
{
    show(g_arc, visible && determinate);
    show(g_spinner, visible && !determinate);
}

void set_texts(const char *headline, const char *detail)
{
    lv_label_set_text(g_headline, headline);
    lv_label_set_text(g_detail, detail);
}

void enter(ViewState state, const char *headline, const char *detail,
           bool offer_start, const char *close_text)
{
    g_state = state;
    g_state_since = millis();
    g_caption_since = g_state_since;
    g_caption_index = 0;

    bool running = (state == VIEW_RUNNING);
    show_progress(running, running && g_known_duration_s > 0);
    // While it runs the headline sits inside the ring; otherwise it stands on
    // its own under the title.
    lv_obj_align(g_headline, LV_ALIGN_TOP_MID, 0, running ? 91 : 56);
    lv_obj_align(g_detail, LV_ALIGN_TOP_MID, 0, running ? 40 : 100);

    set_texts(headline, detail);
    show(g_start_btn, offer_start);
    lv_label_set_text(g_close_lbl, close_text);
    // Centred when it stands alone, beside Start when it does not.
    lv_obj_align(g_close_btn, LV_ALIGN_BOTTOM_MID, offer_start ? 100 : 0, -22);
}

void close_view(void)
{
    g_state = VIEW_HIDDEN;
    show(g_root, false);
    if (g_timer) {
        lv_timer_pause(g_timer);
    }
}

void open_view(void)
{
    if (!g_root || g_state != VIEW_HIDDEN) {
        return;
    }

    show(g_root, true);
    lv_obj_move_foreground(g_root);
    if (g_timer) {
        lv_timer_resume(g_timer);
    }

    // A cycle already running - started here earlier, or from the phone - is
    // worth showing rather than offering to start a second one.
    switch (g_machine_bf) {
        case BF_CLEANING:
            enter(VIEW_RUNNING, "", "", false, "Schliessen");
            return;
        case BF_REQUESTED:
            enter(VIEW_WAITING, "Hebel bewegen",
                  "Die Maschine wartet auf den Hebel", false, "Schliessen");
            return;
        default:
            break;
    }

    if (!g_saw_dashboard) {
        enter(VIEW_BLOCKED, "Keine Daten",
              "Die Maschine hat noch nichts gemeldet", false, "Schliessen");
        return;
    }

    if (!g_saw_widget) {
        enter(VIEW_BLOCKED, "Nicht verfuegbar",
              "Diese Maschine meldet keine Reinigung", false, "Schliessen");
        return;
    }

    if (!g_machine_on) {
        enter(VIEW_BLOCKED, "Maschine ist aus",
              "Zum Reinigen muss sie eingeschaltet sein", false, "Schliessen");
        return;
    }

    enter(VIEW_CONFIRM, "Blindsieb einsetzen",
          "Ohne Blindsieb spritzt das Wasser", true, "Abbrechen");
}

void show_running(void)
{
    unsigned long elapsed_s = (millis() - g_state_since) / 1000;
    char buffer[32];

    if (g_known_duration_s > 0) {
        // The machine decides when it is done, so the countdown is a forecast,
        // not a promise: it stops at zero and waits rather than going negative.
        uint32_t left = (elapsed_s >= g_known_duration_s)
                            ? 0 : (uint32_t)(g_known_duration_s - elapsed_s);
        if (left > 0) {
            snprintf(buffer, sizeof(buffer), "%lu:%02lu",
                     (unsigned long)(left / 60), (unsigned long)(left % 60));
        } else {
            snprintf(buffer, sizeof(buffer), "%s", "gleich");
        }
        int32_t progress = (int32_t)((elapsed_s * 1000) / g_known_duration_s);
        lv_arc_set_value(g_arc, progress > 1000 ? 1000 : progress);
    } else {
        snprintf(buffer, sizeof(buffer), "%lu:%02lu",
                 elapsed_s / 60, elapsed_s % 60);
    }
    lv_label_set_text(g_headline, buffer);

    // A caption that changes now and then, so a screen with nothing else
    // moving on it still looks alive.
    if (millis() - g_caption_since >= 4000) {
        g_caption_since = millis();
        g_caption_index = (uint8_t)((g_caption_index + 1) % CAPTION_COUNT);
    }
    lv_label_set_text(g_detail, CAPTIONS[g_caption_index]);
}

// Keeps what a finished cycle took, so the next one can be counted down. Very
// short runs are not a cycle - the paddle was moved back, or it was cut short.
void remember_duration(unsigned long ran_ms)
{
    uint32_t seconds = (uint32_t)(ran_ms / 1000);
    if (seconds < 30 || seconds > 20 * 60) {
        return;
    }
    if (seconds == g_known_duration_s) {
        return;
    }
    g_known_duration_s = seconds;
    preferences.putUInt("BF_SECS", seconds);
    Serial.printf("[BACKFLUSH] Cycle took %lu s, remembered for the next one\n",
                  (unsigned long)seconds);
}

// Runs inside the LVGL task, which already holds the GUI mutex.
void tick(lv_timer_t *)
{
    if (g_state == VIEW_HIDDEN) {
        return;
    }

    MachineBackflush bf = g_machine_bf;

    switch (g_state) {
        case VIEW_BLOCKED:
        case VIEW_CONFIRM:
            // Someone started it elsewhere while this was on screen.
            if (bf == BF_CLEANING) {
                enter(VIEW_RUNNING, "", "", false, "Schliessen");
            } else if (bf == BF_REQUESTED) {
                enter(VIEW_WAITING, "Hebel bewegen",
                      "Die Maschine wartet auf den Hebel", false, "Schliessen");
            }
            break;

        case VIEW_WAITING:
            if (bf == BF_CLEANING) {
                enter(VIEW_RUNNING, "", "", false, "Schliessen");
                break;
            }
            // The machine gives up on its own if the paddle stays untouched.
            // Nothing announces that, so fall back to a timeout of our own.
            if (bf == BF_OFF &&
                (millis() - g_state_since) >= BACKFLUSH_REQUEST_TIMEOUT_MS) {
                enter(VIEW_DONE, "Nicht gestartet",
                      "Der Hebel wurde nicht bewegt", false, "Schliessen");
            }
            break;

        case VIEW_RUNNING:
            if (bf == BF_OFF) {
                remember_duration(millis() - g_state_since);
                enter(VIEW_DONE, "Fertig", "", false, "Schliessen");
                break;
            }
            show_running();
            break;

        case VIEW_DONE:
            if ((millis() - g_state_since) >= BACKFLUSH_DONE_LINGER_MS) {
                close_view();
            }
            break;

        default:
            break;
    }
}

void on_start(lv_event_t *)
{
    activity_monitor_mark_user_activity();
    machine_action_request_backflush();
    enter(VIEW_WAITING, "Hebel bewegen",
          "Die Maschine wartet auf den Hebel", false, "Schliessen");
}

void on_close(lv_event_t *)
{
    activity_monitor_mark_user_activity();
    close_view();
}

void on_counter_clicked(lv_event_t *)
{
    activity_monitor_mark_user_activity();
    open_view();
}

lv_obj_t *make_button(const char *text, lv_coord_t x, lv_event_cb_t handler,
                      lv_obj_t **label_out)
{
    lv_obj_t *button = lv_btn_create(g_root);
    lv_obj_set_size(button, 180, 54);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, x, -22);
    lv_obj_set_style_radius(button, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(button, handler, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(label);
    if (label_out) {
        *label_out = label;
    }
    return button;
}

}  // namespace

void backflush_view_init(void)
{
    if (g_root || !ui_mainScreen) {
        return;
    }

    const lv_color_t ink   = lv_color_hex(SHOT_VIEW_INK_COLOR);
    const lv_color_t muted = lv_color_hex(SHOT_VIEW_MUTED_COLOR);

    g_root = lv_obj_create(ui_mainScreen);
    lv_obj_set_size(g_root, 536, 240);
    lv_obj_center(g_root);
    lv_obj_set_style_bg_color(g_root, lv_color_hex(SHOT_VIEW_BG_COLOR),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_root, LV_OBJ_FLAG_HIDDEN);

    g_title = lv_label_create(g_root);
    lv_label_set_text(g_title, "BACKFLUSH");
    lv_obj_set_style_text_font(g_title, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_title, muted, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(g_title, LV_ALIGN_TOP_MID, 0, 22);

    g_headline = lv_label_create(g_root);
    lv_obj_set_style_text_font(g_headline, &lv_font_montserrat_30,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_headline, ink, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(g_headline, "");
    lv_obj_align(g_headline, LV_ALIGN_TOP_MID, 0, 56);

    g_detail = lv_label_create(g_root);
    lv_obj_set_style_text_font(g_detail, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_detail, muted, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(g_detail, "");
    lv_obj_align(g_detail, LV_ALIGN_TOP_MID, 0, 100);

    // Shown only while a cycle runs: the ring when its length is known from a
    // previous run, the spinner when it is not.
    g_arc = lv_arc_create(g_root);
    lv_obj_set_size(g_arc, 100, 100);
    lv_obj_align(g_arc, LV_ALIGN_TOP_MID, 0, 58);
    lv_arc_set_rotation(g_arc, 270);
    lv_arc_set_bg_angles(g_arc, 0, 360);
    lv_arc_set_range(g_arc, 0, 1000);
    lv_arc_set_value(g_arc, 0);
    lv_obj_remove_style(g_arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(g_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(g_arc, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(g_arc, 10, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(g_arc, lv_color_hex(BOILER_ARC_COLOR_READY),
                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
    show(g_arc, false);

    g_spinner = lv_spinner_create(g_root, 1400, 70);
    lv_obj_set_size(g_spinner, 100, 100);
    lv_obj_align(g_spinner, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_set_style_arc_width(g_spinner, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(g_spinner, 10, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(g_spinner, lv_color_hex(BOILER_ARC_COLOR_READY),
                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
    show(g_spinner, false);

    g_start_btn = make_button("Starten", -100, on_start, nullptr);
    g_close_btn = make_button("Schliessen", 100, on_close, &g_close_lbl);

    g_known_duration_s = preferences.getUInt("BF_SECS", 0);

    g_timer = lv_timer_create(tick, 250, nullptr);
    lv_timer_pause(g_timer);

    // The counter in the top bar is the backflush counter, so that is where the
    // gesture belongs. Wired here rather than in the generated screen code, so
    // re-exporting from SquareLine Studio cannot drop it.
    lv_obj_t *targets[] = {ui_FlushCountIcon, ui_FlushCountLabel};
    for (lv_obj_t *target : targets) {
        if (!target) {
            continue;
        }
        // Pixel exact hit testing on an icon this small means most taps miss.
        lv_obj_clear_flag(target, LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_add_flag(target, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(target, 14);
        lv_obj_add_event_cb(target, on_counter_clicked, LV_EVENT_CLICKED, nullptr);
    }
}

void backflush_view_set_machine_state(bool powered_on, const char *status)
{
    g_machine_on = powered_on;
    g_saw_dashboard = true;

    if (!status) {
        return;  // widget missing from this message; keep what we had
    }
    g_saw_widget = true;
    if (strcmp(status, "Cleaning") == 0) {
        g_machine_bf = BF_CLEANING;
    } else if (strcmp(status, "Requested") == 0) {
        g_machine_bf = BF_REQUESTED;
    } else {
        g_machine_bf = BF_OFF;
    }
}

bool backflush_view_is_open(void)
{
    return g_state != VIEW_HIDDEN;
}
