#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "lamarzocco_auth.h"
#include "Preferences.h"

struct AccessToken {
    String access_token;
    String refresh_token;
    time_t expires_at = 0;  // Unix timestamp in seconds, 0 when it could not be anchored

    // "now" is passed in rather than read here, so callers cannot accidentally
    // compare this Unix timestamp against a different time base.
    bool isValid(time_t now) const {
        return access_token.length() > 0 && expires_at > now;
    }
};

class LaMarzoccoClient {
public:
    LaMarzoccoClient(Preferences& prefs);
    ~LaMarzoccoClient();
    
    // Initialize client with credentials
    bool init(const String& username, const String& password, const String& serial_number);
    
    // Register client (call after generating installation key)
    bool register_client();
    
    // Get access token (sign in or refresh)
    bool get_access_token();
    
    // Make authenticated API call
    bool api_call(const String& method, const String& endpoint, JsonDocument* request_body, JsonDocument* response_body);
    
    // Get installation key
    bool get_installation_key(InstallationKey& key) const;
    
    // Check if initialized
    bool is_initialized() const { return _initialized; }
    
    // Get serial number
    String get_serial_number() const { return _serial_number; }
    
    // Get access token string (for websocket)
    String get_access_token_string() const { return _access_token.access_token; }

    // HTTP status of the last sign in or token refresh. A 4xx is an answer from
    // the server - the credentials really were rejected. A negative value is a
    // transport error and says nothing about the credentials.
    int get_last_auth_status() const { return _last_auth_status; }
    
private:
    Preferences& _prefs;
    InstallationKey _installation_key;
    AccessToken _access_token;
    String _username;
    String _password;
    String _serial_number;
    bool _initialized;
    int _last_auth_status = 0;
    WiFiClientSecure _client;
    
    // Internal helpers
    bool _sign_in();
    bool _refresh_token();
    void _add_auth_headers(HTTPClient& http);
};

