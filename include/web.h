#pragma once

#include <Arduino.h>

void setupWEB(void);

// WPA2 key of the setup access point. Generated once per device and kept in
// NVS, so it stays the same across reboots. Shown on the setup screen.
String getApPassword(void);

// Marks that the portal wrote new configuration, so it can be applied once the
// user is done instead of waiting for a manual restart.
void portal_mark_config_changed(void);
