#pragma once

// The power and steam buttons show the state of what they switch, rather
// than being fixed images: on, off, or waiting for the machine to confirm a
// tap. See control_buttons.cpp.

typedef enum {
    CONTROL_BUTTON_POWER = 0,
    CONTROL_BUTTON_STEAM,
    CONTROL_BUTTON_COUNT
} ControlButton;

// Call from the LVGL task after ui_init().
void control_buttons_init(void);

// Call from the button's click callback (LVGL task): fades the button until
// the machine reports the switched state, or the wait times out.
void control_buttons_mark_pending(ControlButton button);
