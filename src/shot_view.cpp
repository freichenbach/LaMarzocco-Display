#include "shot_view.h"
#include "config.h"
#include "scale_ble.h"
#include "shot_logger.h"

#include <Arduino.h>
#include "ui/ui.h"

#if SCALE_BLE_ENABLED

namespace {

lv_obj_t *g_root = nullptr;

// Live half
lv_obj_t *g_time_caption = nullptr;
lv_obj_t *g_time_value   = nullptr;
lv_obj_t *g_time_unit    = nullptr;
lv_obj_t *g_weight_caption = nullptr;
lv_obj_t *g_weight_value   = nullptr;
lv_obj_t *g_weight_unit    = nullptr;
lv_obj_t *g_divider      = nullptr;
lv_obj_t *g_flow_caption = nullptr;
lv_obj_t *g_flow_value   = nullptr;

// Result half
lv_obj_t *g_result_value = nullptr;
lv_obj_t *g_result_avg   = nullptr;
lv_obj_t *g_hint         = nullptr;
// Two scales, because the two curves measure different things over different
// ranges: flow in grams per second on the left, weight in grams on the right.
// Each is scaled to its own maximum, so both fill the chart.
lv_obj_t *g_axis_flow[3]   = {nullptr, nullptr, nullptr};
lv_obj_t *g_axis_weight[3] = {nullptr, nullptr, nullptr};
lv_obj_t *g_axis_x[3]      = {nullptr, nullptr, nullptr};
lv_obj_t *g_legend_flow   = nullptr;
lv_obj_t *g_legend_weight = nullptr;

// Shared between both halves: the chart is also the sample store.
lv_obj_t *g_chart = nullptr;
lv_chart_series_t *g_series_flow = nullptr;
lv_chart_series_t *g_series_weight = nullptr;

// The chart only keeps the fixed-point values it draws with. These parallel
// raw samples are what shot_logger.h actually uploads.
ShotLogSample g_raw_samples[SHOT_CHART_POINTS];

bool g_active = false;
bool g_showing_result = false;
bool g_dismiss_requested = false;

uint16_t g_point_index = 0;
uint32_t g_last_sample_ms = 0;
float g_flow_sum = 0.0f;
uint16_t g_flow_samples = 0;
int32_t g_max_flow_x100 = 100;   // never scale below 1.0 g/s, so a weak shot still reads
int32_t g_max_weight_x10 = 200;  // ... and below 20 g
float g_last_weight = 0.0f;

lv_obj_t *make_label(const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(g_root);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "");
    return label;
}

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

void on_root_clicked(lv_event_t *)
{
    if (g_showing_result) {
        g_dismiss_requested = true;
    }
}

void layout_live(void)
{
    show(g_time_caption, true);
    show(g_time_value, true);
    show(g_time_unit, true);
    show(g_weight_caption, true);
    show(g_weight_value, true);
    show(g_weight_unit, true);
    show(g_divider, true);
    show(g_flow_caption, true);
    show(g_flow_value, true);

    show(g_result_value, false);
    show(g_result_avg, false);
    show(g_hint, false);
    show(g_legend_flow, false);
    show(g_legend_weight, false);
    for (int i = 0; i < 3; i++) {
        show(g_axis_flow[i], false);
        show(g_axis_weight[i], false);
        show(g_axis_x[i], false);
    }

    // The weight curve only appears with the result; during the shot the band
    // shows the one value that is actually changing shape.
    lv_chart_hide_series(g_chart, g_series_weight, true);
    lv_obj_set_pos(g_chart, 16, 174);
    lv_obj_set_size(g_chart, 504, 46);
}

void layout_result(void)
{
    show(g_time_caption, false);
    show(g_time_value, false);
    show(g_time_unit, false);
    show(g_weight_caption, false);
    show(g_weight_value, false);
    show(g_weight_unit, false);
    show(g_divider, false);
    show(g_flow_caption, false);
    show(g_flow_value, false);

    show(g_result_value, true);
    show(g_result_avg, true);
    show(g_hint, true);
    show(g_legend_flow, true);
    show(g_legend_weight, true);
    for (int i = 0; i < 3; i++) {
        show(g_axis_flow[i], true);
        show(g_axis_weight[i], true);
        show(g_axis_x[i], true);
    }

    lv_chart_hide_series(g_chart, g_series_weight, false);
    // Narrower than the screen on both sides: the flow scale stands to the
    // left of it, the weight scale to the right.
    lv_obj_set_pos(g_chart, 44, 56);
    lv_obj_set_size(g_chart, 424, 144);
}

}  // namespace

void shot_view_init(void)
{
    if (g_root || !ui_mainScreen) {
        return;
    }

    g_root = lv_obj_create(ui_mainScreen);
    lv_obj_set_size(g_root, 536, 240);
    lv_obj_center(g_root);
    lv_obj_set_style_bg_color(g_root, lv_color_hex(SHOT_VIEW_BG_COLOR), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_root, on_root_clicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(g_root, LV_OBJ_FLAG_HIDDEN);

    const lv_color_t ink   = lv_color_hex(SHOT_VIEW_INK_COLOR);
    const lv_color_t muted = lv_color_hex(SHOT_VIEW_MUTED_COLOR);
    const lv_color_t flow  = lv_color_hex(SHOT_FLOW_COLOR);
    const lv_color_t mass  = lv_color_hex(SHOT_WEIGHT_COLOR);

    // Chart first, so the labels draw on top of it.
    g_chart = lv_chart_create(g_root);
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_chart, SHOT_CHART_POINTS);
    lv_chart_set_div_line_count(g_chart, 3, 0);
    lv_chart_set_update_mode(g_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_size(g_chart, 0, LV_PART_INDICATOR);  // no dots, just the line
    lv_obj_set_style_border_width(g_chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_chart, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(g_chart, muted, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(g_chart, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(g_chart, 3, LV_PART_ITEMS | LV_STATE_DEFAULT);

    g_series_flow   = lv_chart_add_series(g_chart, flow, LV_CHART_AXIS_PRIMARY_Y);
    g_series_weight = lv_chart_add_series(g_chart, mass, LV_CHART_AXIS_SECONDARY_Y);

    // --- live ---
    g_time_caption = make_label(&lv_font_montserrat_14, muted);
    lv_label_set_text(g_time_caption, "ZEIT");
    lv_obj_align(g_time_caption, LV_ALIGN_TOP_MID, -134, 52);

    g_time_value = make_label(&lv_font_montserrat_48, ink);
    lv_obj_align(g_time_value, LV_ALIGN_TOP_MID, -146, 74);
    g_time_unit = make_label(&lv_font_montserrat_20, muted);
    lv_label_set_text(g_time_unit, "s");

    g_weight_caption = make_label(&lv_font_montserrat_14, muted);
    lv_label_set_text(g_weight_caption, "GEWICHT");
    lv_obj_align(g_weight_caption, LV_ALIGN_TOP_MID, 134, 52);

    g_weight_value = make_label(&lv_font_montserrat_48, ink);
    lv_obj_align(g_weight_value, LV_ALIGN_TOP_MID, 122, 74);
    g_weight_unit = make_label(&lv_font_montserrat_20, muted);
    lv_label_set_text(g_weight_unit, "g");

    g_divider = lv_obj_create(g_root);
    lv_obj_set_size(g_divider, 1, 110);
    lv_obj_align(g_divider, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(g_divider, muted, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_divider, LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_divider, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_divider, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_flow_caption = make_label(&lv_font_montserrat_12, muted);
    lv_label_set_text(g_flow_caption, "FLUSS");
    lv_obj_align(g_flow_caption, LV_ALIGN_TOP_LEFT, 16, 156);

    g_flow_value = make_label(&lv_font_montserrat_14, muted);
    lv_obj_align(g_flow_value, LV_ALIGN_TOP_RIGHT, -16, 154);

    // --- result ---
    g_result_value = make_label(&lv_font_montserrat_30, ink);
    lv_obj_align(g_result_value, LV_ALIGN_TOP_LEFT, 16, 8);

    g_result_avg = make_label(&lv_font_montserrat_14, muted);
    lv_obj_align(g_result_avg, LV_ALIGN_TOP_RIGHT, -16, 12);

    // The legend names the unit as well as the colour, so the numbers on the
    // two scales need no further explanation.
    g_legend_flow = make_label(&lv_font_montserrat_12, flow);
    lv_label_set_text(g_legend_flow, "Fluss g/s");
    lv_obj_align(g_legend_flow, LV_ALIGN_TOP_LEFT, 16, 32);

    g_legend_weight = make_label(&lv_font_montserrat_12, mass);
    lv_label_set_text(g_legend_weight, "Gewicht g");
    lv_obj_align(g_legend_weight, LV_ALIGN_TOP_LEFT, 96, 32);

    for (int i = 0; i < 3; i++) {
        // Each scale carries its curve's colour, so which number belongs to
        // which line needs no looking up.
        g_axis_flow[i] = make_label(&lv_font_montserrat_12, flow);
        lv_obj_align(g_axis_flow[i], LV_ALIGN_TOP_LEFT, 16, 50 + i * 68);

        g_axis_weight[i] = make_label(&lv_font_montserrat_12, mass);
        lv_obj_align(g_axis_weight[i], LV_ALIGN_TOP_LEFT, 476, 50 + i * 68);

        // The chart spans x 44..468, so the three ticks sit at its start,
        // middle and end rather than on an arbitrary grid.
        g_axis_x[i] = make_label(&lv_font_montserrat_12, muted);
        lv_obj_align(g_axis_x[i], LV_ALIGN_TOP_LEFT, 40 + i * 212, 204);
    }

    g_hint = make_label(&lv_font_montserrat_12, muted);
    lv_label_set_text(g_hint, "Antippen schliesst");
    lv_obj_align(g_hint, LV_ALIGN_BOTTOM_MID, 0, -2);

    layout_live();
}

bool shot_view_available(void)
{
    bookoo::Reading reading;
    uint32_t age_ms = 0;
    return g_root != nullptr &&
           scale_ble_is_connected() &&
           scale_ble_last_reading(reading, age_ms) &&
           age_ms < SCALE_READING_STALE_MS;
}

void shot_view_start(void)
{
    if (!g_root) {
        return;
    }

    // The result view shrinks the chart to the samples the shot actually
    // produced, so give it its full capacity back before filling it again.
    lv_chart_set_point_count(g_chart, SHOT_CHART_POINTS);
    lv_chart_set_all_value(g_chart, g_series_flow, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(g_chart, g_series_weight, LV_CHART_POINT_NONE);
    g_point_index = 0;
    g_last_sample_ms = millis();
    g_flow_sum = 0.0f;
    g_flow_samples = 0;
    g_max_flow_x100 = 100;
    g_max_weight_x10 = 200;
    g_last_weight = 0.0f;
    g_dismiss_requested = false;
    g_showing_result = false;
    g_active = true;

    lv_label_set_text(g_time_value, "0.0");
    lv_label_set_text(g_weight_value, "0.0");
    lv_label_set_text(g_flow_value, "0.00 g/s");

    layout_live();
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_root);
}

void shot_view_tick(int64_t elapsed_ms)
{
    if (!g_active || g_showing_result) {
        return;
    }

    bookoo::Reading reading;
    uint32_t age_ms = 0;
    if (!scale_ble_last_reading(reading, age_ms) || age_ms >= SCALE_READING_STALE_MS) {
        return;  // the caller decides whether to fall back to the plain timer
    }

    g_last_weight = reading.weight_g;
    g_flow_sum += reading.flow_g_per_s;
    g_flow_samples++;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.1f", (float)(elapsed_ms / 100) / 10.0f);
    lv_label_set_text(g_time_value, buffer);
    lv_obj_align_to(g_time_unit, g_time_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);

    snprintf(buffer, sizeof(buffer), "%.1f", reading.weight_g);
    lv_label_set_text(g_weight_value, buffer);
    lv_obj_align_to(g_weight_unit, g_weight_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);

    snprintf(buffer, sizeof(buffer), "%.2f g/s", reading.flow_g_per_s);
    lv_label_set_text(g_flow_value, buffer);

    uint32_t now = millis();
    if (now - g_last_sample_ms < SHOT_SAMPLE_INTERVAL_MS || g_point_index >= SHOT_CHART_POINTS) {
        return;
    }
    g_last_sample_ms = now;

    float flow = (g_flow_samples > 0) ? (g_flow_sum / g_flow_samples) : 0.0f;
    g_flow_sum = 0.0f;
    g_flow_samples = 0;

    int32_t flow_x100 = (int32_t)(flow * 100.0f);
    int32_t weight_x10 = (int32_t)(reading.weight_g * 10.0f);
    if (flow_x100 < 0) flow_x100 = 0;
    if (weight_x10 < 0) weight_x10 = 0;
    if (flow_x100 > g_max_flow_x100) g_max_flow_x100 = flow_x100;
    if (weight_x10 > g_max_weight_x10) g_max_weight_x10 = weight_x10;

    g_raw_samples[g_point_index] = { reading.weight_g, flow };

    lv_chart_set_value_by_id(g_chart, g_series_flow, g_point_index, flow_x100);
    lv_chart_set_value_by_id(g_chart, g_series_weight, g_point_index, weight_x10);
    g_point_index++;

    lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 0, g_max_flow_x100);
    lv_chart_refresh(g_chart);
}

float shot_view_settle(void)
{
    bookoo::Reading reading;
    uint32_t age_ms = 0;
    if (g_active && !g_showing_result &&
        scale_ble_last_reading(reading, age_ms) && age_ms < SCALE_READING_STALE_MS) {
        g_last_weight = reading.weight_g;

        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%.1f", reading.weight_g);
        lv_label_set_text(g_weight_value, buffer);
        lv_obj_align_to(g_weight_unit, g_weight_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);
    }
    return g_last_weight;
}

void shot_view_finish(int64_t elapsed_ms)
{
    if (!g_active) {
        return;
    }

    float seconds = (float)(elapsed_ms / 100) / 10.0f;
    float average = (seconds > 0.1f) ? (g_last_weight / seconds) : 0.0f;

    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%.1f g   %.1f s", g_last_weight, seconds);
    lv_label_set_text(g_result_value, buffer);

    // Plain ASCII: LVGL's built-in Montserrat carries no glyph for a diameter
    // sign, and a missing one draws as an empty box.
    snprintf(buffer, sizeof(buffer), "Schnitt %.2f g/s", average);
    lv_label_set_text(g_result_avg, buffer);

    // Both scales name values their own curve actually reaches, top, middle
    // and bottom.
    for (int i = 0; i < 3; i++) {
        snprintf(buffer, sizeof(buffer), "%.1f", (g_max_flow_x100 / 100.0f) * (2 - i) / 2.0f);
        lv_label_set_text(g_axis_flow[i], buffer);

        snprintf(buffer, sizeof(buffer), "%.0f", (g_max_weight_x10 / 10.0f) * (2 - i) / 2.0f);
        lv_label_set_text(g_axis_weight[i], buffer);

        snprintf(buffer, sizeof(buffer), "%.0f", seconds * i / 2.0f);
        lv_label_set_text(g_axis_x[i], buffer);
    }

    lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 0, g_max_flow_x100);
    lv_chart_set_range(g_chart, LV_CHART_AXIS_SECONDARY_Y, 0, g_max_weight_x10);

    // The chart is built to hold a minute of brewing. A shot of ten seconds
    // fills a sixth of it, and the curves were drawn into that sixth while the
    // time labels underneath still spanned the whole width - so the axis said
    // one thing and the picture another. Cutting the chart down to the samples
    // that exist spreads them over the full width, where the labels are.
    lv_chart_set_point_count(g_chart, g_point_index < 2 ? 2 : g_point_index);

    g_showing_result = true;
    layout_result();
    lv_chart_refresh(g_chart);

    // Handed off for an asynchronous upload - see shot_logger.h for why this
    // cannot POST directly from here (the LVGL task, holding gui_mutex).
    const char *target_temp_label = ui_CoffeeTempLabel ? lv_label_get_text(ui_CoffeeTempLabel) : nullptr;
    shot_logger_capture(g_raw_samples, g_point_index, seconds, g_last_weight, average, target_temp_label);
}

void shot_view_hide(void)
{
    if (!g_root) {
        return;
    }
    lv_obj_add_flag(g_root, LV_OBJ_FLAG_HIDDEN);
    g_active = false;
    g_showing_result = false;
    g_dismiss_requested = false;
}

bool shot_view_is_active(void)
{
    return g_active;
}

bool shot_view_take_dismiss_request(void)
{
    bool requested = g_dismiss_requested;
    g_dismiss_requested = false;
    return requested;
}

#else  // SCALE_BLE_ENABLED

void shot_view_init(void) {}
bool shot_view_available(void) { return false; }
void shot_view_start(void) {}
void shot_view_tick(int64_t) {}
float shot_view_settle(void) { return 0.0f; }
void shot_view_finish(int64_t) {}
void shot_view_hide(void) {}
bool shot_view_is_active(void) { return false; }
bool shot_view_take_dismiss_request(void) { return false; }

#endif
