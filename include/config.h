#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"

#ifdef DEBUG
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

// Captive portal redirection
#define REDIRECT_URL "http://192.168.4.1/"
static constexpr const char *NTP_SERVER = "pool.ntp.org";

#define  BATTERY_VOLTAGE_PIN 4

// The hardware cannot tell whether a battery is attached: with none connected
// the reading on BATTERY_VOLTAGE_PIN still sits near 4 V, so the display shows
// a full battery that does not exist. Set to 1 when running on a battery.
#define BATTERY_ICON_ENABLED  0
// Debug aid: pulling GPIO 15 low fakes a brewing cycle on the display. Off by
// default so a release build cannot be triggered by whatever else is wired to
// that pin; enable with -D BREWING_SIM_ENABLED.
#define  BREWING_SIM_PIN 15  // GPIO 15 for brewing simulation mode (LOW = brewing, HIGH = normal)

// Deep sleep saves battery, but this board cannot tell whether it is on battery
// at all, so a device on a power supply simply switches itself off overnight and
// has to be woken with the BOOT button. Off by default; set to 1 for battery use.
#define DEEP_SLEEP_ENABLED  0

// The brewing timer is started and stopped by cloud messages. If the stop never
// arrives - a dropped connection, a lost message - the timer would run forever.
// After this long it clears itself.
#define BREWING_MAX_SECONDS  180

// How often the full machine state is fetched over REST. The WebSocket only
// pushes on change, so this bounds how stale the display can get when a message
// is lost. 0 disables it.
#define DASHBOARD_REFRESH_INTERVAL_MS  (5UL * 60UL * 1000UL)

// Steam boiler: the Micra reports a level, not a temperature. These are the
// approximate temperatures the official app shows for Level1/2/3.
#define STEAM_LEVEL_AS_TEMPERATURE  1
#define STEAM_TEMP_LEVEL1  126
#define STEAM_TEMP_LEVEL2  128
#define STEAM_TEMP_LEVEL3  131

// Boiler arcs: heating up, and ready.
#define BOILER_ARC_COLOR_HEATING  0x4040FF
#define BOILER_ARC_COLOR_READY    0x2FBF4F

#define USER_INACTIVITY_TIMEOUT_MS  (360UL * 60UL * 1000UL)
#define MACHINE_INACTIVITY_TIMEOUT_MS  (360UL * 60UL * 1000UL)
#define USER_DIM_TIMEOUT_MS  (10UL * 60UL * 1000UL)
#define MACHINE_DIM_TIMEOUT_MS  (10UL * 60UL * 1000UL)
#define DISPLAY_BRIGHTNESS_ACTIVE  180
#define DISPLAY_BRIGHTNESS_DIM  30
#define DISPLAY_ROTATION  2

// Setup access point: length of the generated WPA2 key and how long the portal
// keeps running with nobody connected before saved settings are applied
#define AP_PASSWORD_LENGTH  10
#define AP_PORTAL_TIMEOUT_MS  (15UL * 60UL * 1000UL)

// How often the startup sign in is retried before giving up, and the pause
// between attempts. Only transport errors are retried; a rejected credential
// is final.
#define AUTH_ATTEMPTS_AT_STARTUP  3
#define AUTH_RETRY_DELAY_MS  (3UL * 1000UL)

// Time to wait for the NTP sync that TLS certificate validation depends on
#define TIME_SYNC_TIMEOUT_MS  (15UL * 1000UL)

#define uS_TO_S_FACTOR 1000000ULL

#endif
