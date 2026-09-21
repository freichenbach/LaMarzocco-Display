#include <WiFi.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include "Preferences.h"
#include "config.h"
#include "web.h"
#include "wifi_power.h"
#include "web_handle.h"

extern Preferences preferences;

uint64_t timer = 0;

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);

static String ap_password;
static volatile bool portal_config_changed = false;
static volatile bool portal_running = false;
static volatile bool portal_start_requested = false;

void portal_mark_config_changed(void)
{
    portal_config_changed = true;
}

bool isPortalRunning(void)
{
    return portal_running;
}

void requestPortalStart(void)
{
    portal_start_requested = true;
}

void servicePortalRequest(void)
{
    if (!portal_start_requested || portal_running) {
        return;
    }
    portal_start_requested = false;
    Serial.println("[AP] Setup portal requested, starting access point");
    setupWEB();
}

String getApPassword(void)
{
    // Cached after the first call. That call usually happens while the access
    // point is started, but the setup screen may ask for the key first, so this
    // has to work from the LVGL task as well - NVS does its own locking.
    if (ap_password.length() >= 8) {
        return ap_password;
    }

    ap_password = preferences.getString("AP_PASS", "");
    if (ap_password.length() < 8) {
        // The key is read off the display, so leave out characters that are
        // easy to confuse: 0/O, 1/l/i.
        static const char alphabet[] = "23456789abcdefghjkmnpqrstuvwxyz";
        const size_t alphabet_len = sizeof(alphabet) - 1;

        ap_password = "";
        for (int i = 0; i < AP_PASSWORD_LENGTH; i++) {
            ap_password += alphabet[esp_random() % alphabet_len];
        }
        preferences.putString("AP_PASS", ap_password);
    }

    return ap_password;
}

void setupAP()
{
    log_i("Configuring access point...");

    // WPA2 instead of an open network: the portal carries the WiFi password and
    // the La Marzocco credentials, and an open access point offers no link layer
    // encryption at all, so anything in range could read them off the air.
    String password = getApPassword();
    WiFi.softAP(AP_SSID, password.c_str());
    Serial.print("[AP] SSID: " AP_SSID "  Key: ");
    Serial.println(password);
    wifi_apply_power_save();
    delay(100);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    if (!MDNS.begin(AP_SSID)) // using same name as SSID, shottimer.local
        debugln("Error setting up MDNS responder!");
    else
        debugln("mDNS responder started");
    log_i("The hotspot has been established");
}

void webTask(void *args)
{
    unsigned long last_station_ms = millis();

    while (1)
    {
        dnsServer.processNextRequest();
        server.handleClient();

        // Settings only take effect after a restart, which the status page asks
        // the user to trigger. If they saved something and simply walked away,
        // apply it once nobody is connected to the portal any more.
        if (WiFi.softAPgetStationNum() > 0) {
            last_station_ms = millis();
        } else if (portal_config_changed &&
                   (millis() - last_station_ms) >= AP_PORTAL_TIMEOUT_MS) {
            Serial.println("[AP] Portal idle, restarting to apply the saved configuration");
            delay(100);
            ESP.restart();
        }

        vTaskDelay(10); // allow the cpu to switch to other tasks
    }
    vTaskDelete(NULL);
}

void setupWEB(void)
{
    if (portal_running) {
        debugln("Setup portal already running");
        return;
    }

    setupAP();
    initFS();

    // // load css
    server.on("/styles.css", HTTP_GET, cssHandler);

    // load HTML pages
    server.on("/", HTTP_GET, mainHandler);
    server.on("/ssids", HTTP_GET, sendSSID);
    server.on("/statusData", HTTP_GET, sendStatus);
    server.on("/wifiConfig", HTTP_POST, saveWifiHandler);
    server.on("/cloudConfig", HTTP_POST, saveCloudHandler);
    server.on("/machineConfig", HTTP_POST, saveMachineHandler);

    server.on("/restart", HTTP_GET, restartHander);

    server.onNotFound(handleNotFound); // for unhandled cases

    server.begin();
    log_i("HTTP server started");
    timer = millis();

    portal_running = true;

    TaskHandle_t t1;
    //changed this as per Gemini as it could be causing the web server crash.
    //xTaskCreatePinnedToCore((void (*)(void *))webTask, "webTask", 8192, NULL, 10, &t1, 0);
    // Increased stack to 16k, lowered priority to 1, moved to Core 1
    xTaskCreatePinnedToCore((void (*)(void *))webTask, "webTask", 16384, NULL, 1, &t1, 1);
}
