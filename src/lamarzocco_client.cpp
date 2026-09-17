#include "lamarzocco_client.h"
#include "config.h"
#include "lamarzocco_tls.h"
#include <time.h>

static const char* BASE_URL = "lion.lamarzocco.io";
static const char* CUSTOMER_APP_URL = "https://lion.lamarzocco.io/api/customer-app";
static const unsigned long TOKEN_TIME_TO_REFRESH = 10 * 60;  // 10 minutes

// Unix time, or 0 while the clock has not been set yet. Token expiry used to be
// compared against millis()/1000 in some places and against Unix time in others,
// which meant an expired token could look valid whenever NTP had not answered.
static time_t current_epoch()
{
    time_t now = time(nullptr);
    return (now >= LM_MIN_VALID_EPOCH) ? now : 0;
}

// HTTPClient returns a negative code when the request never reached the server,
// which is also what a rejected server certificate looks like. The regular error
// paths use debug() and are compiled out in release builds, so report this case
// unconditionally - otherwise a trust store mismatch would fail silently.
static void log_connection_failure(const char* what, int http_code)
{
    if (http_code >= 0) {
        return;
    }
    Serial.printf("[TLS] %s could not connect: %s (%d)\n",
                  what, HTTPClient::errorToString(http_code).c_str(), http_code);
    Serial.println("[TLS] If this persists, the server certificate may no longer chain "
                   "to a root in src/lamarzocco_tls.cpp");
}

LaMarzoccoClient::LaMarzoccoClient(Preferences& prefs) 
    : _prefs(prefs), _initialized(false) {
    lm_tls_apply(_client);  // Verify the cloud certificate against the bundled roots
}

LaMarzoccoClient::~LaMarzoccoClient() {
}

bool LaMarzoccoClient::init(const String& username, const String& password, const String& serial_number) {
    _username = username;
    _password = password;
    _serial_number = serial_number;
    
    // Load installation key
    if (!LaMarzoccoAuth::load_installation_key(_prefs, _installation_key)) {
        debugln("No installation key found, need to generate one");
        return false;
    }
    
    _initialized = true;
    return true;
}

bool LaMarzoccoClient::get_installation_key(InstallationKey& key) const {
    if (!_initialized) {
        return false;
    }
    key = _installation_key;
    return true;
}

bool LaMarzoccoClient::register_client() {
    if (!_initialized) {
        debugln("Client not initialized");
        return false;
    }
    
    String base_string = LaMarzoccoAuth::generate_base_string(_installation_key);
    String proof = LaMarzoccoAuth::generate_request_proof(base_string, _installation_key.secret);
    String public_key_b64 = LaMarzoccoAuth::base64_encode(_installation_key.public_key_der, _installation_key.public_key_len);
    
    HTTPClient http;
    http.begin(_client, String(CUSTOMER_APP_URL) + "/auth/init");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-App-Installation-Id", _installation_key.installation_id);
    http.addHeader("X-Request-Proof", proof);
    
    JsonDocument request;
    request["pk"] = public_key_b64;
    
    String request_body;
    serializeJson(request, request_body);
    
    int http_code = http.POST(request_body);
    String response = http.getString();
    http.end();
    
    if (http_code == 200 || http_code == 201) {
        debugln("Registration successful");
        return true;
    } else {
        log_connection_failure("Registration", http_code);
        debug("Registration failed: ");
        debugln(http_code);
        debugln(response);
        return false;
    }
}

bool LaMarzoccoClient::_sign_in() {
    JsonDocument request;
    request["username"] = _username;
    request["password"] = _password;
    
    String request_body;
    serializeJson(request, request_body);
    
    HTTPClient http;
    http.begin(_client, String(CUSTOMER_APP_URL) + "/auth/signin");
    http.addHeader("Content-Type", "application/json");
    _add_auth_headers(http);
    
    int http_code = http.POST(request_body);
    String response = http.getString();
    http.end();
    
    if (http_code == 200) {
        JsonDocument response_doc;
        deserializeJson(response_doc, response);
        
        _access_token.access_token = response_doc["accessToken"].as<String>();
        _access_token.refresh_token = response_doc["refreshToken"].as<String>();
        
        unsigned long expires_in = response_doc["expiresIn"].as<unsigned long>();
        time_t now = current_epoch();
        // Leaving this at 0 when the clock is unset makes the next call fetch a
        // fresh token rather than trust an expiry it cannot place in time.
        _access_token.expires_at = now ? (now + (time_t)expires_in) : 0;
        
        debugln("Sign in successful");
        return true;
    } else {
        log_connection_failure("Sign in", http_code);
        debug("Sign in failed: ");
        debugln(http_code);
        debugln(response);
        return false;
    }
}

bool LaMarzoccoClient::_refresh_token() {
    if (_access_token.refresh_token.length() == 0) {
        return _sign_in();
    }
    
    JsonDocument request;
    request["username"] = _username;
    request["refreshToken"] = _access_token.refresh_token;
    
    String request_body;
    serializeJson(request, request_body);
    
    HTTPClient http;
    http.begin(_client, String(CUSTOMER_APP_URL) + "/auth/refreshtoken");
    http.addHeader("Content-Type", "application/json");
    _add_auth_headers(http);
    
    int http_code = http.POST(request_body);
    String response = http.getString();
    http.end();
    
    if (http_code == 200) {
        JsonDocument response_doc;
        deserializeJson(response_doc, response);
        
        _access_token.access_token = response_doc["accessToken"].as<String>();
        if (response_doc.containsKey("refreshToken")) {
            _access_token.refresh_token = response_doc["refreshToken"].as<String>();
        }
        
        unsigned long expires_in = response_doc["expiresIn"].as<unsigned long>();
        time_t now = current_epoch();
        // Leaving this at 0 when the clock is unset makes the next call fetch a
        // fresh token rather than trust an expiry it cannot place in time.
        _access_token.expires_at = now ? (now + (time_t)expires_in) : 0;
        
        debugln("Token refresh successful");
        return true;
    } else {
        log_connection_failure("Token refresh", http_code);
        debug("Token refresh failed: ");
        debugln(http_code);
        return _sign_in();  // Fallback to sign in
    }
}

bool LaMarzoccoClient::get_access_token() {
    if (!_initialized) {
        return false;
    }
    
    time_t now = current_epoch();
    if (now == 0) {
        // The clock is not set, so nothing can be said about expiry. Use the
        // token we have instead of signing in on every call - the TLS handshake
        // needs the clock as well and will fail loudly if it is really unset.
        return _access_token.access_token.length() > 0 ? true : _sign_in();
    }

    if (_access_token.isValid(now) &&
        _access_token.expires_at >= now + (time_t)TOKEN_TIME_TO_REFRESH) {
        return true;
    }

    if (_access_token.refresh_token.length() > 0 && _access_token.expires_at > now) {
        return _refresh_token();
    }

    return _sign_in();
}

void LaMarzoccoClient::_add_auth_headers(HTTPClient& http) {
    String installation_id, timestamp, nonce, signature;
    LaMarzoccoAuth::generate_extra_request_headers(_installation_key, installation_id, timestamp, nonce, signature);
    
    http.addHeader("X-App-Installation-Id", installation_id);
    http.addHeader("X-Timestamp", timestamp);
    http.addHeader("X-Nonce", nonce);
    http.addHeader("X-Request-Signature", signature);
}

bool LaMarzoccoClient::api_call(const String& method, const String& endpoint, JsonDocument* request_body, JsonDocument* response_body) {
    String url = String(CUSTOMER_APP_URL) + endpoint;

    String request_str;
    if (request_body) {
        serializeJson(*request_body, request_str);
    }

    // Two attempts: a token can be rejected even though it still looked current
    // here, for instance after it was revoked server side. Without the retry a
    // single 401 fails the call and the caller sees it as a lost connection.
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!get_access_token()) {
            return false;
        }

        HTTPClient http;
        http.begin(_client, url);
        http.addHeader("Content-Type", "application/json");
        _add_auth_headers(http);
        http.addHeader("Authorization", "Bearer " + _access_token.access_token);

        int http_code = 0;
        if (method == "GET") {
            http_code = http.GET();
        } else if (method == "POST") {
            http_code = http.POST(request_str);
        } else if (method == "PUT") {
            http_code = http.PUT(request_str);
        } else if (method == "DELETE") {
            http_code = http.sendRequest("DELETE", request_str);
        } else {
            http.end();
            return false;
        }

        String response_str = http.getString();
        http.end();

        if (http_code >= 200 && http_code < 300) {
            if (response_body && response_str.length() > 0) {
                deserializeJson(*response_body, response_str);
            }
            return true;
        }

        if (http_code == 401 && attempt == 0) {
            debugln("API call rejected the access token, fetching a new one");
            // Clearing only the access token keeps the refresh token, so the
            // next get_access_token() refreshes instead of signing in again.
            _access_token.access_token = "";
            continue;
        }

        log_connection_failure("API call", http_code);
        debug("API call failed: ");
        debugln(http_code);
        debugln(response_str);
        return false;
    }

    return false;
}

