#pragma once

// A tiny line-based command console over the USB serial port, typed into the
// PlatformIO/VS Code serial monitor (`pio device monitor`). Lets settings
// that normally only go through the setup portal's web forms be written with
// the device plugged in over USB and nothing else running - no WiFi access
// point needed.
//
// Commands (newline-terminated):
//   set <KEY> <value>   writes <value> under <KEY> in Preferences
//   get <KEY>           prints the current value of <KEY>, or "not set"
//
// <KEY> must match an existing Preferences key exactly, e.g. HA_WEBHOOK -
// NVS keys are case sensitive, this does not normalise case.
//
// Call from the main Arduino loop() - not the LVGL task.
void serial_console_loop(void);
