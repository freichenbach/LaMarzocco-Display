#pragma once

#include <Arduino.h>
#include "lamarzocco_client.h"
#include "lamarzocco_websocket.h"

class LaMarzoccoMachine {
public:
    LaMarzoccoMachine(LaMarzoccoClient& client, LaMarzoccoWebSocket& websocket);
    
    // Set power on/off
    bool set_power(bool enabled);
    
    // Get current power state (from last command or websocket update)
    bool get_power_state() const { return _power_state; }

    // False until the machine has actually reported its status. Until then the
    // power and steam flags are only defaults, and a toggle built on them can
    // send the opposite of what the user meant.
    bool has_reported_status() const { return _status_reported; }
    
    // Toggle power
    bool toggle_power();
    
    // Set steam boiler on/off
    bool set_steam(bool enabled);
    
    // Get current steam boiler state
    bool get_steam_state() const { return _steam_state; }
    
    // Toggle steam boiler
    bool toggle_steam();
    
    // Connect websocket and start listening
    bool connect_websocket();
    
    // Check if websocket is connected
    bool is_websocket_connected() const;
    
    // Disconnect websocket
    void disconnect_websocket();
    
    // Loop (call in main loop)
    void loop();

    // Request a refresh of coffee/flush counters
    void request_stats_refresh();

    // Fetch the full machine state over REST. The WebSocket only pushes on
    // change, so without this the display keeps whatever it last heard - and a
    // message lost on a weak link is never made good.
    bool refresh_dashboard();
    
private:
    LaMarzoccoClient& _client;
    LaMarzoccoWebSocket& _websocket;
    bool _power_state;
    bool _steam_state;
    bool _status_reported = false;
    bool _stats_refresh_pending = false;
    unsigned long _last_stats_refresh_ms = 0;
    unsigned long _last_dashboard_refresh_ms = 0;
    
    // WebSocket message handler
    static void _websocket_message_handler(const String& message);
    static void _process_dashboard(JsonDocument& doc);
    static LaMarzoccoMachine* _instance;

    void _refresh_shot_counters();
};
