#pragma once

// A brief status line on the main screen reporting the outcome of the last
// shot_logger upload - success and failure both get a visible line, not
// just the Serial log, which nobody is watching during normal use.
//
// Created at runtime directly on ui_mainScreen (not in the generated screen
// code), so re-exporting the screens from SquareLine Studio cannot drop it.

// Shows `message` for SHOT_UPLOAD_STATUS_MS, in green for success or red for
// failure. Safe to call from any task - takes the GUI mutex itself. Not for
// the LVGL task (shot_logger_loop() runs from the main Arduino loop()).
void shot_upload_status_show(bool success, const char *message);

// Clears the line once its time is up. Call from the main Arduino loop().
void shot_upload_status_loop(void);
