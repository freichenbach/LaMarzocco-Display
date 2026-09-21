#include <Arduino.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <ui/ui.h>
#include "Preferences.h"
#include "web.h"
#include <WiFi.h>
#include "config.h"
#include "update_screen.h"
#include "lamarzocco_client.h"
#include "lamarzocco_websocket.h"
#include "lamarzocco_machine.h"
#include "lamarzocco_auth.h"
#include "boiler_display.h"
#include "water_alarm.h"
#include "brewing_display.h"
#include "activity_monitor.h"
#include "lamarzocco_tls.h"
#include "machine_actions.h"
#include "scale_ble.h"

Preferences preferences;
LaMarzoccoClient* g_client = nullptr;
LaMarzoccoWebSocket* g_websocket = nullptr;
LaMarzoccoMachine* g_machine = nullptr;

LilyGo_Class amoled;
SemaphoreHandle_t gui_mutex;
void Task_LVGL(void *pvParameters);
void updateSerialLoggingPowerState(bool force);

// WiFi connection variables
const int MAX_WIFI_RETRIES = 10;
const int WIFI_TIMEOUT_MS = 15000;

static const char *authModeName(wifi_auth_mode_t mode)
{
  switch (mode) {
    case WIFI_AUTH_OPEN:            return "open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    default:                        return "unknown";
  }
}

// The access point states why it refused the association. Without this the
// firmware can only report that the connection did not happen, which looks
// identical for a wrong password, a network on 5 GHz and an access point that
// requires an authentication mode the ESP32 does not speak.
static void logWiFiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info)
{
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    Serial.printf("[WIFI] Association refused, reason %u\n",
                  info.wifi_sta_disconnected.reason);
  }
}

// Called once the retries are used up: is the configured network in range at
// all, and if so, what does it require?
static void reportWiFiFailure(const String &ssid)
{
  Serial.printf("[WIFI] Giving up, status %d\n", WiFi.status());

  int found = WiFi.scanNetworks();
  bool seen = false;
  for (int i = 0; i < found; i++) {
    if (WiFi.SSID(i) == ssid) {
      seen = true;
      Serial.printf("[WIFI] '%s' on %s: channel %d, %d dBm, security %s\n",
                    ssid.c_str(), WiFi.BSSIDstr(i).c_str(), (int)WiFi.channel(i),
                    (int)WiFi.RSSI(i), authModeName(WiFi.encryptionType(i)));
    }
  }
  if (!seen) {
    Serial.printf("[WIFI] '%s' is not among the %d networks in range - this "
                  "radio only sees 2.4 GHz.\n", ssid.c_str(), found);
  }
  WiFi.scanDelete();
}

bool connectToWiFi(const String &ssid, const String &password)
{
  debugln("Attempting to connect to WiFi...");

  static bool disconnect_logging_registered = false;
  if (!disconnect_logging_registered) {
    WiFi.onEvent(logWiFiDisconnect);
    disconnect_logging_registered = true;
  }

  // The default scan stops at the first access point answering to the network
  // name and connects to that one, however weak it is. In a building with one
  // access point per floor that is regularly the wrong one - a device three
  // metres from an access point ended up on a distant one at -98 dBm, losing
  // beacons and failing every TLS handshake. Scanning all channels makes the
  // sort below apply, so the strongest one wins.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  WiFi.begin(ssid.c_str(), password.c_str());
  WiFi.setSleep(false);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < MAX_WIFI_RETRIES)
  {
    delay(WIFI_TIMEOUT_MS / MAX_WIFI_RETRIES);
    debug(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    debugln("");
    debugln("WiFi connected!");
    debug("IP address: ");
    debugln(WiFi.localIP());
    // Which access point was picked, so a weak one is visible immediately
    // rather than as a string of odd failures further along.
    Serial.printf("[WIFI] Connected to %s, channel %d, %d dBm\n",
                  WiFi.BSSIDstr().c_str(), (int)WiFi.channel(), (int)WiFi.RSSI());
    return true;
  }
  else
  {
    debugln("");
    debugln("Failed to connect to WiFi");
    WiFi.disconnect();
    reportWiFiFailure(ssid);
    return false;
  }
}

// LilyGo_AMOLED::isVbusIn() is only implemented for the board variants that
// carry a power management chip. On the others - including the 1.91" QSPI board
// this firmware is built for - it returns false unconditionally, which is
// indistinguishable from "running on battery".
static bool boardReportsVbus()
{
  switch (amoled.getBoardID()) {
    case LILYGO_AMOLED_147:
    case LILYGO_AMOLED_241:
    case LILYGO_AMOLED_191_SPI:
      return true;
    default:
      return false;
  }
}

void updateSerialLoggingPowerState(bool force)
{
  static bool serial_enabled = true;
  static unsigned long last_check_ms = 0;
  unsigned long now = millis();

  // Without a usable VBUS reading this would shut down USB serial on a device
  // that is plainly USB powered, and never turn it back on - the serial monitor
  // would be gone for good on exactly the board this targets.
  if (!boardReportsVbus()) {
    return;
  }

  if (!force && (now - last_check_ms) < 5000) {
    return;
  }
  last_check_ms = now;

  bool usb_powered = amoled.isVbusIn();
  if (usb_powered && !serial_enabled) {
    Serial.begin(115200);
    serial_enabled = true;
  } else if (!usb_powered && serial_enabled) {
    Serial.flush();
    Serial.end();
    serial_enabled = false;
  }
}

//function to enter deep sleep mode.  This helps to save power when device not in use and using a battery.
void enterDeepSleep() {
    Serial.println("Preparing to sleep...");
    
    // 1. Turn off the display so you know it worked
    amoled.setBrightness(0);
    
    // 2. CRITICAL: Wait for button release!
    // This loop blocks the code until you let go of the button.
    // Otherwise, the device sleeps and wakes up instantly.
    while (digitalRead(0) == LOW) {
        delay(10);
    }
    
    // 3. Small debounce delay to ensure the signal is clean
    delay(100);

    Serial.println("Goodnight!");

    // 4. Configure Wakeup Source
    // Wake up when GPIO 0 goes LOW (Pressed again)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
    
    // 5. Enter Deep Sleep
    esp_deep_sleep_start();
}

void setup()
{
  Serial.begin(115200);

#ifdef DEBUG
  // This board speaks USB directly from the ESP32-S3, so the CDC device
  // re-enumerates when the application takes over from the bootloader. The
  // host's serial monitor needs a moment to reattach and misses everything
  // printed until then - which is most of setup(), including why the WiFi
  // connection failed. Wait for the host to reopen the port, capped so a
  // device running without a monitor is not held up.
  unsigned long serial_wait_started = millis();
  while (!Serial && (millis() - serial_wait_started) < 3000) {
    delay(10);
  }
  delay(300);  // let the monitor settle before the first lines go out
  Serial.println();
  Serial.println("[BOOT] Starting up");
#endif

  preferences.begin("config", false);
  pinMode(0, INPUT_PULLUP);

  bool rslt = false;

  // Automatically determine the access device
  rslt = amoled.begin();

  if (!rslt)
  {
    while (1)
    {
      debug("The board model cannot be detected, please raise the Core Debug Level to an error");
      delay(1000);
    }
  }

  amoled.setRotation(DISPLAY_ROTATION);

  updateSerialLoggingPowerState(true);

  activity_monitor_init(USER_INACTIVITY_TIMEOUT_MS, MACHINE_INACTIVITY_TIMEOUT_MS);

  gui_mutex = xSemaphoreCreateMutex();
  if (gui_mutex == NULL)
  {
    // Handle semaphore creation failure
    log_i("gui_mutex semaphore creation failure");
    return;
  }

  xTaskCreatePinnedToCore(Task_LVGL,
                          "Task_LVGL",
                          1024 * 16,  // Increased for crypto operations
                          NULL,
                          3,
                          NULL,
                          0);

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  delay(500);

  String ssid = preferences.getString("SSID", "");
  String pass = preferences.getString("PASS", "");
  if (ssid == "" || pass == "")
  {
    debugln("No WiFi credentials found, starting WiFi setup");
    lv_disp_load_scr(ui_NoConnectionScreen);
    setupWEB();
  }
  else
  {
    debugln("Found WiFi credentials");
    if (connectToWiFi(ssid, pass))
    {
      lv_disp_load_scr(ui_mainScreen);

      // NTP only starts syncing once WiFi is up. The TLS handshake with the
      // cloud checks the certificate dates, so wait for a valid clock before
      // the first request instead of failing verification on a 1970 date.
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
      if (lm_tls_wait_for_clock(TIME_SYNC_TIMEOUT_MS)) {
        debugln("Clock synchronized via NTP");
      } else {
        Serial.println("[TLS] NTP sync timed out - certificate validation may fail");
      }

      // Initialize La Marzocco client
      String email = preferences.getString("USER_EMAIL", "");
      String password = preferences.getString("USER_PASS", "");
      String machine_serial = preferences.getString("MACHINE", "");
      
      if (email.length() > 0 && password.length() > 0 && machine_serial.length() > 0) {
        debugln("Initializing La Marzocco client...");
        
        // Check if installation key exists, if not generate it first
        InstallationKey key;
        if (!LaMarzoccoAuth::load_installation_key(preferences, key)) {
          debugln("Generating installation key...");
          
          // Clear any partial keys that might exist (check before removing to avoid errors)
          if (preferences.isKey("INSTALLATION_ID")) preferences.remove("INSTALLATION_ID");
          if (preferences.isKey("INSTALLATION_SECRET")) preferences.remove("INSTALLATION_SECRET");
          if (preferences.isKey("INSTALLATION_PRIVKEY")) preferences.remove("INSTALLATION_PRIVKEY");
          if (preferences.isKey("INSTALLATION_PUBKEY")) preferences.remove("INSTALLATION_PUBKEY");
          if (preferences.isKey("INSTALLATION_PRIVKEY_LEN")) preferences.remove("INSTALLATION_PRIVKEY_LEN");
          if (preferences.isKey("INSTALLATION_PUBKEY_LEN")) preferences.remove("INSTALLATION_PUBKEY_LEN");
          if (preferences.isKey("INST_ID")) preferences.remove("INST_ID");
          if (preferences.isKey("INST_SECRET")) preferences.remove("INST_SECRET");
          if (preferences.isKey("INST_PRIVKEY")) preferences.remove("INST_PRIVKEY");
          if (preferences.isKey("INST_PUBKEY")) preferences.remove("INST_PUBKEY");
          if (preferences.isKey("INST_PRIVLEN")) preferences.remove("INST_PRIVLEN");
          if (preferences.isKey("INST_PUBLEN")) preferences.remove("INST_PUBLEN");
          
          String installation_id = LaMarzoccoAuth::generate_uuid();
          if (LaMarzoccoAuth::generate_installation_key(installation_id, key)) {
            if (LaMarzoccoAuth::save_installation_key(preferences, key)) {
              debugln("Installation key generated and saved");
            } else {
              debugln("Failed to save installation key");
            }
          } else {
            debugln("Failed to generate installation key");
          }
        } else {
          debugln("Installation key found");
        }
        
        g_client = new LaMarzoccoClient(preferences);
        if (g_client->init(email, password, machine_serial)) {
          // Register client if needed
          debugln("Registering client...");
          if (!g_client->register_client()) {
            debugln("Registration failed - will retry on first API call");
            // Note: Registration failures are not critical, will retry during API calls
          }
          
          // Try to get access token (authenticate). A single attempt used to be
          // enough to declare the credentials invalid and drop into the setup
          // portal, so one timed out request during startup cost the whole
          // session - and told the user their password was wrong.
          bool authorized = false;
          for (int attempt = 1; attempt <= AUTH_ATTEMPTS_AT_STARTUP; attempt++) {
            authorized = g_client->get_access_token();
            if (authorized) {
              break;
            }

            int status = g_client->get_last_auth_status();
            Serial.printf("[AUTH] Sign in attempt %d of %d failed, status %d\n",
                          attempt, AUTH_ATTEMPTS_AT_STARTUP, status);

            // A 4xx is the server answering that it rejected the credentials.
            // Retrying cannot change that; anything else is worth another try.
            if (status >= 400 && status < 500) {
              break;
            }
            if (attempt < AUTH_ATTEMPTS_AT_STARTUP) {
              delay(AUTH_RETRY_DELAY_MS);
            }
          }

          if (!authorized) {
            int status = g_client->get_last_auth_status();
            if (status >= 400 && status < 500) {
              debugln("Authorization failed - the server rejected the credentials");
              showNoConnectionScreen(
                "Authorization Failed!\n"
                "Invalid credentials\n"
                "Please restart WiFi Setup"
              );

              delete g_client;
              g_client = nullptr;
              setupWEB();
            } else {
              // The cloud could not be reached. The credentials may be perfectly
              // fine, so keep the client: every later API call signs in again on
              // its own, and the device recovers without being reconfigured.
              Serial.println("[AUTH] Cloud unreachable at startup, will keep retrying");
              showNoConnectionScreen(
                "Cloud Unreachable!\n"
                "Could not sign in\n"
                "Retrying..."
              );
            }
          } else {
            // Initialize websocket and machine
            g_websocket = new LaMarzoccoWebSocket(*g_client);
            g_machine = new LaMarzoccoMachine(*g_client, *g_websocket);
            
            debugln("La Marzocco client initialized");

            // Initial refresh of coffee/flush counters
            g_machine->request_stats_refresh();

            // Bluetooth comes up after WiFi so the radio is already settled.
            scale_ble_begin();
            
            // Auto-connect WebSocket on startup
            debugln("Auto-connecting to WebSocket...");
            if (g_machine->connect_websocket()) {
              debugln("✓ WebSocket connection initiated on startup");
            } else {
              debugln("✗ Failed to initiate WebSocket connection on startup");
              // Note: WebSocket failures are not critical, will retry automatically
            }
          }
        } else {
          debugln("Failed to initialize La Marzocco client");
          
          showNoConnectionScreen(
            "Client Init Failed!\n"
            "Missing installation key\n"
            "Please restart WiFi Setup"
          );
          
          delete g_client;
          g_client = nullptr;
          setupWEB();
        }
      } else {
        debugln("Missing La Marzocco credentials");
        // Note: Missing credentials is expected on first run, no error message needed
      }
    }
    else
    {
      debugln("WiFi connection failed after retries, starting WiFi setup");
      lv_disp_load_scr(ui_NoConnectionScreen);
      setupWEB();
    }
  }
}

void loop()
{
  servicePortalRequest();   // starts the setup portal when the UI asked for it
  scale_ble_loop();         // finds and reconnects the BOOKOO scale
  machine_actions_process();  // runs power/steam requests from the UI buttons
  updateDateTime();
  updateStatusImages();  // Update battery and WiFi images (initial + every 30 seconds)
  checkWiFiConnection(); // Monitor WiFi connection and redirect if disconnected
  updateSerialLoggingPowerState(false);
  
  // Check GPIO 15 for brewing simulation mode
  brewing_display_check_gpio_simulation();
  
  // Handle websocket and machine loop - MUST be called frequently
  // WebSocket requires regular loop() calls to process messages
  if (g_machine) {
    g_machine->loop();  // This calls websocket.loop()
  }
  
  // Small delay to prevent watchdog issues, but keep loop responsive
  delay(10);
  
  // Fast reconnection check (every 5 seconds)
  static unsigned long last_check = 0;
  static unsigned long last_reconnect_attempt = 0;
  static const unsigned long CHECK_INTERVAL = 5000;     // Check every 5 seconds
  static const unsigned long RECONNECT_INTERVAL = 10000; // Try reconnect every 10 seconds
  
  if (millis() - last_check > CHECK_INTERVAL) {
    last_check = millis();
    if (g_machine) {
      if (g_machine->is_websocket_connected()) {
        // Connected - only log occasionally to reduce noise
        static unsigned long last_log = 0;
        if (millis() - last_log > 60000) { // Log every 60 seconds when connected
          Serial.println("[STATUS] ✓ WebSocket connected");
          last_log = millis();
        }
      } else {
        // Disconnected - try to reconnect quickly
        if (millis() - last_reconnect_attempt > RECONNECT_INTERVAL) {
          last_reconnect_attempt = millis();
          Serial.println("[RECONNECT] WebSocket disconnected, reconnecting...");
          
          // Try to reconnect
          if (g_machine->connect_websocket()) {
            Serial.println("[RECONNECT] ✓ Reconnection initiated");
          } else {
            Serial.println("[RECONNECT] ✗ Reconnection failed, will retry in 10s");
          }
        }
      }
    }
  }

  static bool display_dimmed = false;
  unsigned long last_user_ms = activity_monitor_last_user_ms();
  unsigned long last_machine_ms = activity_monitor_last_machine_ms();
  unsigned long now = millis();
  bool user_dim_inactive = (USER_DIM_TIMEOUT_MS > 0) &&
                           (now >= last_user_ms) &&
                           (static_cast<uint32_t>(now - last_user_ms) >= USER_DIM_TIMEOUT_MS);
  bool machine_dim_inactive = (MACHINE_DIM_TIMEOUT_MS > 0) &&
                              (now >= last_machine_ms) &&
                              (static_cast<uint32_t>(now - last_machine_ms) >= MACHINE_DIM_TIMEOUT_MS);
  bool should_dim = user_dim_inactive && machine_dim_inactive;
  if (should_dim && !display_dimmed) {
    amoled.setBrightness(DISPLAY_BRIGHTNESS_DIM);
    display_dimmed = true;
  } else if (!should_dim && display_dimmed) {
    amoled.setBrightness(DISPLAY_BRIGHTNESS_ACTIVE);
    display_dimmed = false;
  }

#if DEEP_SLEEP_ENABLED
  bool user_inactive = activity_monitor_is_user_inactive(now);
  bool machine_inactive = activity_monitor_is_machine_inactive(now);
  if (user_inactive && machine_inactive) {
    Serial.print("[SLEEP] Inactivity timeout: ");
    Serial.println("user + machine");
    enterDeepSleep();
  }
#endif
  // Check if BOOT button (GPIO 0) is held down to turn OFF
    // (GPIO 0 is LOW when pressed)
    if (digitalRead(0) == LOW) {
        delay(100); // Debounce
        unsigned long startTime = millis();
        
        // Wait to see if user holds it for 2 seconds
        while (digitalRead(0) == LOW) {
            if (millis() - startTime > 2000) {
                // User held it for 2 seconds -> SLEEP
                enterDeepSleep(); 
            }
        }
    }
}

void Task_LVGL(void *pvParameters)
{
  beginLvglHelper(amoled);
  ui_init();
  
  // Initialize boiler display system after UI is ready
  boiler_display_set_mutex((void*)gui_mutex);  // Set mutex for thread-safe LVGL access
  boiler_display_init();
  
  // Initialize water alarm display system
  water_alarm_set_mutex((void*)gui_mutex);
  water_alarm_init();
  
  // Initialize brewing display system
  brewing_display_set_mutex((void*)gui_mutex);
  brewing_display_init();
  
  // Main LVGL loop
  while (1)
  {
    if (xSemaphoreTake(gui_mutex, portMAX_DELAY) == pdTRUE)
    {
      lv_timer_handler();
      xSemaphoreGive(gui_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
