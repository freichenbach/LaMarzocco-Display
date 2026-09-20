#include "ui/ui.h"
#include "lamarzocco_machine.h"
#include "activity_monitor.h"
#include "web.h"
#include "machine_actions.h"

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

enum PendingAction : uint8_t {
  PENDING_NONE = 0,
  PENDING_POWER,
  PENDING_STEAM,
};

static volatile PendingAction g_pending_action = PENDING_NONE;

void machine_action_request_power_toggle(void)
{
  g_pending_action = PENDING_POWER;
}

void machine_action_request_steam_toggle(void)
{
  g_pending_action = PENDING_STEAM;
}

static void run_machine_action(PendingAction action)
{
  if (!g_machine) {
    Serial.println("[ACTION] No machine client - not configured?");
    return;
  }

  const char *what = (action == PENDING_POWER) ? "power" : "steam boiler";
  Serial.printf("[ACTION] Toggling %s\n", what);

  if (!g_machine->is_websocket_connected()) {
    Serial.println("[ACTION] WebSocket not connected, connecting first");
    if (g_machine->connect_websocket()) {
      delay(1000);  // give the connection a moment to come up
    } else {
      Serial.println("[ACTION] Could not initiate the WebSocket connection");
    }
  }

  bool success = (action == PENDING_POWER) ? g_machine->toggle_power()
                                           : g_machine->toggle_steam();

  // No UI update here: the button state follows the WebSocket confirmation,
  // which also avoids taking the GUI mutex from this side.
  Serial.printf("[ACTION] %s toggle %s\n", what, success ? "sent" : "failed");
}

void machine_actions_process(void)
{
  PendingAction action = g_pending_action;
  if (action == PENDING_NONE) {
    return;
  }
  g_pending_action = PENDING_NONE;
  run_machine_action(action);
}

// Both callbacks below run in the LVGL task and must return quickly, so they
// only note the request. See include/machine_actions.h.
void turnOnMachine(lv_event_t * e)
{
  activity_monitor_mark_user_activity();
  machine_action_request_power_toggle();
}

void toggleSteamBoiler(lv_event_t * e)
{
  activity_monitor_mark_user_activity();
  machine_action_request_steam_toggle();
}
