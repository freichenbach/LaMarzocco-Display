#pragma once

#include "bookoo_protocol.h"

#include <stdint.h>

// BLE client for a BOOKOO Themis scale.
//
// Bluetooth and WiFi share the same radio on this chip. On a device with a
// weak WiFi link, running both will cost some of what is left, so this can be
// turned off entirely with SCALE_BLE_ENABLED in config.h.

// Starts scanning. Safe to call when the feature is disabled - it does nothing.
void scale_ble_begin(void);

// Drives connecting and reconnecting. Call from loop().
void scale_ble_loop(void);

bool scale_ble_is_connected(void);

// Most recent reading, with how long ago it arrived. False when nothing has
// been received yet.
bool scale_ble_last_reading(bookoo::Reading &out, uint32_t &age_ms);

// Sends a command to the scale. Nothing calls this during a shot - the scale
// handles taring and its own timer by itself - but the channel is here for
// commands that have to come from the display.
bool scale_ble_send(bookoo::Command command, int value = 0);
