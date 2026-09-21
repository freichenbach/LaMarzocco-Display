#pragma once

#include <stdint.h>

// A log that survives being unplugged.
//
// The machine stands where there is no computer, so the interesting part - a
// whole shot, or a crash during one - happens with nobody reading the serial
// port. Lines written here are kept in flash and printed over serial the next
// time the device starts at a desk.
//
// Writes go into a small buffer in RAM first, so a call from the LVGL task
// never waits for flash; shot_log_flush() from the main loop does the writing.

void shot_log_begin(void);

// Appends one line, timestamped with the seconds since start. Safe from any
// task.
void shot_log_printf(const char *fmt, ...);

// Writes what is buffered. Call from loop().
void shot_log_flush(void);

// Prints the stored log over serial and empties it.
void shot_log_dump_and_clear(void);
