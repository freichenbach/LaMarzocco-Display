#pragma once

// The cleaning cycle, started from the display instead of the phone.
//
// Opened by tapping the flush counter in the top bar - that counter is the
// backflush counter, so the gesture sits where the subject already is, and
// nothing on the main screen has to give up room for it.
//
// The machine does not start on the command alone: it asks for the paddle to
// be moved and reports the cycle as Requested until that happens. The view
// follows those three stages rather than guessing.

// Builds the view and makes the flush counter tappable. Call from the LVGL
// task, after ui_init().
void backflush_view_init(void);

// What the machine last reported. Call from the main loop when a dashboard
// arrives; status may be null when the machine did not send the widget.
// Records only - the view picks it up in its own timer, so this never touches
// LVGL and needs no mutex.
void backflush_view_set_machine_state(bool powered_on, const char *status);

bool backflush_view_is_open(void);
