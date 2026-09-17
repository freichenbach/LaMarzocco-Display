#pragma once

#include <Arduino.h>

// Starts the setup access point and the configuration portal. Does nothing if
// the portal is already running.
void setupWEB(void);

// The portal can also be started at runtime, e.g. after WiFi was lost while the
// device was already running. requestPortalStart() is safe to call from the LVGL
// task; servicePortalRequest() does the actual work and belongs in the main loop,
// because setupWEB() creates tasks and sockets.
void requestPortalStart(void);
void servicePortalRequest(void);
bool isPortalRunning(void);

// WPA2 key of the setup access point. Generated once per device and kept in
// NVS, so it stays the same across reboots. Shown on the setup screen.
String getApPassword(void);

// Marks that the portal wrote new configuration, so it can be applied once the
// user is done instead of waiting for a manual restart.
void portal_mark_config_changed(void);
