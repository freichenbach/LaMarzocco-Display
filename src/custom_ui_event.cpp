#include "ui/ui.h"
#include "lamarzocco_machine.h"
#include "activity_monitor.h"
#include "web.h"

extern LaMarzoccoMachine* g_machine;

// The setup access point is WPA2 protected with a key generated per device, so
// the key has to be readable somewhere. This label is created here rather than
// in ui_setupWifiScreen.c so that re-exporting the screen from SquareLine
// Studio does not drop it again.
static lv_obj_t* ui_APKeyLabel = nullptr;

void wifiSetup(lv_event_t *e)
{
    activity_monitor_mark_user_activity();
    lv_label_set_text(ui_SSIDLabel, "SSID: " AP_SSID);
    lv_label_set_text(ui_URLLabel, "URL:  http://" AP_SSID ".local");

    if (!ui_APKeyLabel) {
        ui_APKeyLabel = lv_label_create(ui_setupWifiScreen);
        lv_obj_set_style_text_font(ui_APKeyLabel, &lv_font_montserrat_20,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    // Runs inside the LVGL event handler, which already holds the GUI mutex.
    String key_text = String("Key:  ") + getApPassword();
    lv_label_set_text(ui_APKeyLabel, key_text.c_str());
    lv_obj_align_to(ui_APKeyLabel, ui_URLLabel, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    // The screen shows the network to connect to, so the access point has to be
    // running. It is not when WiFi was lost after startup: setupWEB() is only
    // called from setup(). Hand the request to the main loop, which can create
    // the tasks and sockets that setupWEB() needs.
    if (!isPortalRunning()) {
        requestPortalStart();
    }

    lv_scr_load(ui_setupWifiScreen);
}

void turnOnMachine(lv_event_t * e)
{
  activity_monitor_mark_user_activity();
  // Get the machine control instance
  if (g_machine) {
    Serial.println("===========================================");
    Serial.println("BUTTON PRESSED - Processing...");
    Serial.println("===========================================");
    
    // Check current websocket status
    if (g_machine->is_websocket_connected()) {
      Serial.println("✓ WebSocket is already connected");
    } else {
      Serial.println("⚠ WebSocket not connected, attempting to connect...");
      bool connected = g_machine->connect_websocket();
      if (connected) {
        Serial.println("✓ WebSocket connection initiated");
        // Give it a moment to establish
        delay(1000);
      } else {
        Serial.println("✗ Failed to initiate WebSocket connection");
      }
    }
    
    // Toggle the power
    Serial.println("\nToggling machine power...");
    bool success = g_machine->toggle_power();
    if (success) {
      Serial.println("✓ Power toggle command sent successfully");
      Serial.println("Check WebSocket messages below for confirmation...");
    } else {
      Serial.println("✗ Failed to send power toggle command");
    }
    
    Serial.println("===========================================\n");
  } else {
    Serial.println("ERROR: g_machine is null!");
  }
}

void toggleSteamBoiler(lv_event_t * e)
{
  activity_monitor_mark_user_activity();
  // Get the machine control instance
  if (g_machine) {
    Serial.println("===========================================");
    Serial.println("STEAM BUTTON PRESSED - Processing...");
    Serial.println("===========================================");
    
    // Check current websocket status
    if (g_machine->is_websocket_connected()) {
      Serial.println("✓ WebSocket is already connected");
    } else {
      Serial.println("⚠ WebSocket not connected, attempting to connect...");
      bool connected = g_machine->connect_websocket();
      if (connected) {
        Serial.println("✓ WebSocket connection initiated");
        // Give it a moment to establish
        delay(1000);
      } else {
        Serial.println("✗ Failed to initiate WebSocket connection");
      }
    }
    
    // Toggle the steam boiler
    Serial.println("\nToggling steam boiler...");
    bool success = g_machine->toggle_steam();
    if (success) {
      Serial.println("✓ Steam boiler toggle command sent successfully");
      Serial.println("WebSocket will confirm state change...");
      // Note: No UI update here - button state will update when WebSocket
      // confirms the change. This avoids mutex deadlock.
    } else {
      Serial.println("✗ Failed to send steam boiler toggle command");
    }
    
    Serial.println("===========================================\n");
  } else {
    Serial.println("ERROR: g_machine is null!");
  }
}
