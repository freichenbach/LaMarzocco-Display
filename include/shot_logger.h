#pragma once

#include <stdint.h>

// Uploads a finished shot's curve to a local logging endpoint (a Home
// Assistant webhook), so it survives past the SHOT_RESULT_MS the display
// shows it for. Optional: with no URL configured, capture and loop become
// no-ops.

// One BOOKOO scale sample captured during a shot. Plain floats so the raw
// values can be serialised, rather than the chart's fixed-point storage.
struct ShotLogSample {
    float weight_g;
    float flow_g_per_s;
};

// Copies a finished shot for later, asynchronous upload. Cheap and does no
// network I/O, so it is safe to call from the LVGL task. A shot that was
// captured but not yet sent is replaced by the next one - this device logs
// the latest shot, not a queue of them.
//
// target_temp_label is the coffee boiler's target temperature as already
// shown on screen (e.g. "94°C"), or NULL if not known yet.
void shot_logger_capture(const ShotLogSample *samples, uint16_t sample_count,
                          float duration_s, float final_weight_g,
                          float avg_flow_g_per_s, const char *target_temp_label);

// Sends a captured shot if one is pending. Called from the main Arduino
// loop() - never from the LVGL task, since the HTTP request blocks.
void shot_logger_loop(void);
