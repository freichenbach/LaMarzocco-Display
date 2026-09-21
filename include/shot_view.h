#pragma once

#include <stdbool.h>
#include <stdint.h>

// The brewing view used when a scale is connected: time and weight side by
// side while the shot runs, the flow and weight curves afterwards.
//
// Everything is built at runtime rather than in the generated screen code, so
// re-exporting the screens from SquareLine Studio cannot drop it. All calls
// must come from the LVGL task, which is where the brewing timer runs.

void shot_view_init(void);

// True when a scale is connected and its readings are current. Asked at the
// start of each shot to choose the layout for that shot, and again while one
// runs: a scale that stops reporting mid shot falls back to the plain timer.
bool shot_view_available(void);

// The shot started. Shows time, weight and the live flow band.
void shot_view_start(void);

// Called from the brewing timer while the shot runs.
void shot_view_tick(int64_t elapsed_ms);

// The shot ended: switch to the result and the curves.
void shot_view_finish(int64_t elapsed_ms);

// Back to the normal screen.
void shot_view_hide(void);

bool shot_view_is_active(void);

// True once the user tapped the result away. Clears the flag.
bool shot_view_take_dismiss_request(void);
