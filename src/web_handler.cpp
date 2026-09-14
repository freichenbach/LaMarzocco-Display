#include "web_handle.h"
#include "FS.h"
#include "SPIFFS.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "Preferences.h"
#include "lamarzocco_auth.h"
#include "web.h"
#include <set>

extern Preferences preferences;
extern WebServer server;

struct CaseInsensitiveCompare
{
    bool operator()(const String &a, const String &b) const
    {
        String lowerA = a;
        lowerA.toLowerCase();
        String lowerB = b;
        lowerB.toLowerCase();
        return lowerA < lowerB;
    }
};

static bool fs_mounted = false;

// Shown instead of a blank page when the filesystem image was never flashed.
// The regular logging here is compiled out in release builds, so without this
// the portal would just answer with nothing and look like broken hardware.
static void sendMissingFilesystemPage(const String &path)
{
    Serial.printf("[FS] %s is not available in SPIFFS\n", path.c_str());
    server.send(500, "text/html",
                "<h2>Web interface not installed</h2>"
                "<p>The filesystem image is missing from this device. Flash it with"
                " <code>pio run --target uploadfs</code> and restart.</p>");
}

void initFS(void)
{
    fs_mounted = SPIFFS.begin();
    if (!fs_mounted) {
        Serial.println("[FS] SPIFFS mount failed - the web interface will not be available.");
        Serial.println("[FS] Flash the filesystem image with: pio run --target uploadfs");
    }
}

void streamFile(String path)
{
    File file = fs_mounted ? SPIFFS.open(path, "r") : File();
    if (!file)
    {
        sendMissingFilesystemPage(path);
        return;
    }
    server.streamFile(file, "text/html");
    file.close();
}

void handleNotFound(void)
{
    server.sendHeader("Location", REDIRECT_URL, true);
    server.send(302, "text/plain", "");
}

void cssHandler(void)
{
    File CSSfile = fs_mounted ? SPIFFS.open("/styles.css", "r") : File();
    if (!CSSfile)
    {
        // The pages stay readable without styling, so answer empty rather than
        // with the error page, which would end up inside a <link> tag.
        server.send(404, "text/css", "");
        return;
    }
    server.streamFile(CSSfile, "text/css");
    CSSfile.close();
}

void mainHandler(void)
{
    streamFile("/main.html");
}

void sendSSID(void)
{
    int n = WiFi.scanNetworks(); // Scan for available networks
    if (n == 0)
        Serial.println("No networks found");
    else
        Serial.println("Networks found:");
    std::set<String, CaseInsensitiveCompare> ssidSet;
    for (int i = 0; i < n; ++i)
        ssidSet.insert(WiFi.SSID(i));

    JsonDocument jsonDoc;
    JsonArray ssidArray = jsonDoc.to<JsonArray>();
    for (const auto &ssid : ssidSet)
    {
        ssidArray.add(ssid);
    }

    String jsonString;
    serializeJson(jsonDoc, jsonString);
    server.send(200, "application/json", jsonString);
}

// The status page only needs to show what is configured, not the values
// themselves. Anyone who reaches the portal can read this, so keep the account
// name and the machine serial recognisable but incomplete.
static String maskEmail(const String &email)
{
    int at = email.indexOf('@');
    if (email.length() == 0) return "N/A";
    if (at <= 0) return "***";
    return email.substring(0, 1) + "***" + email.substring(at);
}

static String maskSerial(const String &serial)
{
    if (serial.length() == 0) return "N/A";
    if (serial.length() <= 4) return "***";
    return "***" + serial.substring(serial.length() - 4);
}

void sendStatus(void)
{
    JsonDocument jsonDoc;
    jsonDoc["wifi"] = preferences.getString("SSID", "N/A");
    jsonDoc["email"] = maskEmail(preferences.getString("USER_EMAIL", ""));
    jsonDoc["machine"] = maskSerial(preferences.getString("MACHINE", ""));
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    server.send(200, "application/json", jsonString);
}

// Nothing limits the size of a submitted field, and an over-long value makes
// putString() fail without telling anyone - the setting then silently stays at
// its old value. Limits follow the protocols: 32 for an SSID, 63 for a WPA
// passphrase, 254 for an e-mail address.
static bool storeField(const char *key, const String &value, size_t max_len)
{
    if (value.length() > max_len) {
        Serial.printf("[WEB] %s rejected: %u characters, at most %u allowed\n",
                      key, (unsigned)value.length(), (unsigned)max_len);
        server.send(400, "text/html",
                    "<h2>Value too long</h2>"
                    "<p>One of the submitted values exceeds the allowed length."
                    " Please go back and correct it.</p>");
        return false;
    }
    preferences.putString(key, value);
    return true;
}

void saveWifiHandler(void)
{
    String ssid = server.arg("ssid");
    if (ssid == "OTHERS")
        ssid = server.arg("manual_ssid");

    if (!storeField("SSID", ssid, 32)) return;
    if (!storeField("PASS", server.arg("password"), 63)) return;

    portal_mark_config_changed();
    streamFile("/credential.html");
}

void saveCloudHandler(void)
{
    if (!storeField("USER_EMAIL", server.arg("user_email"), 254)) return;
    if (!storeField("USER_PASS", server.arg("user_pass"), 128)) return;

    portal_mark_config_changed();
    streamFile("/machine.html");
}

void saveMachineHandler(void)
{
    if (!storeField("MACHINE", server.arg("machine"), 32)) return;
    portal_mark_config_changed();
    
    // Generate installation key if not exists
    InstallationKey key;
    if (!LaMarzoccoAuth::load_installation_key(preferences, key)) {
        debugln("Generating new installation key...");
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
        debugln("Installation key already exists");
    }
    
    streamFile("/status.html");
}

void restartHander(void)
{
    server.send(200);
    delay(1000);
    ESP.restart();
}
