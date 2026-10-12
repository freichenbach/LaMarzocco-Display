#include "shot_logger.h"
#include "config.h"
#include "shot_upload_status.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <cmath>
#include <esp_random.h>

extern Preferences preferences;

namespace {

ShotLogSample g_samples[SHOT_CHART_POINTS];
uint16_t g_sample_count = 0;
float g_duration_s = 0.0f;
float g_final_weight_g = 0.0f;
float g_avg_flow_g_per_s = 0.0f;
char g_target_temp_label[16] = "";
bool g_pending = false;

// Unix time as an ISO-8601 UTC timestamp, or a millis()-based fallback while
// NTP has not answered yet - either way a usable, sortable shot id.
String make_shot_id()
{
    time_t now = time(nullptr);
    if (now > 8 * 3600 * 2) {  // past 1970-01-01, i.e. NTP has set the clock
        struct tm utc;
        gmtime_r(&now, &utc);
        char buffer[24];
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
        return String(buffer);
    }
    // millis() restarts at 0 on every boot, so two shots pulled this early
    // after two different reboots (a flaky WiFi/power situation, repeatedly
    // rebooting before NTP answers) could otherwise land on the same id and
    // overwrite each other in InfluxDB. The random prefix makes that a
    // non-issue without needing the clock.
    return "boot" + String(esp_random(), HEX) + "-" + String(millis());
}

// ArduinoJson writes NaN/Infinity as bare, non-standard tokens that break
// strict JSON parsers - cheap to guard against here since only flow_g_per_s
// is passed through from the scale without having gone through a divide
// that is already guarded (unlike the average in shot_view.cpp).
// Also rounds to 2 decimals: full float precision (e.g. "0.779091") adds
// nothing useful here and bloats the payload once SHOT_SAMPLE_INTERVAL_MS
// gives a shot several hundred samples.
float finite_or(float value, float fallback)
{
    float v = std::isfinite(value) ? value : fallback;
    return roundf(v * 100.0f) / 100.0f;
}

}  // namespace

void shot_logger_capture(const ShotLogSample *samples, uint16_t sample_count,
                          float duration_s, float final_weight_g,
                          float avg_flow_g_per_s, const char *target_temp_label)
{
    if (sample_count > SHOT_CHART_POINTS) {
        sample_count = SHOT_CHART_POINTS;
    }
    memcpy(g_samples, samples, sample_count * sizeof(ShotLogSample));
    g_sample_count = sample_count;
    g_duration_s = duration_s;
    g_final_weight_g = final_weight_g;
    g_avg_flow_g_per_s = avg_flow_g_per_s;
    if (target_temp_label) {
        strncpy(g_target_temp_label, target_temp_label, sizeof(g_target_temp_label) - 1);
        g_target_temp_label[sizeof(g_target_temp_label) - 1] = '\0';
    } else {
        g_target_temp_label[0] = '\0';
    }
    g_pending = true;
}

void shot_logger_loop(void)
{
    if (!g_pending) {
        return;
    }
    g_pending = false;  // sent at most once - a failed shot is not retried

    String webhook_url = preferences.getString("HA_WEBHOOK", "");
    if (webhook_url.length() == 0) {
        return;  // logging not configured
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[ShotLogger] No WiFi, dropping shot log");
        shot_upload_status_show(false, "Shot-Upload: kein WLAN");
        return;
    }

    JsonDocument doc;
    doc["shot_id"] = make_shot_id();
    doc["duration_s"] = g_duration_s;
    doc["final_weight_g"] = g_final_weight_g;
    doc["avg_flow_g_per_s"] = g_avg_flow_g_per_s;
    if (g_target_temp_label[0]) {
        doc["target_temp_label"] = g_target_temp_label;
    }

    JsonArray samples = doc["samples"].to<JsonArray>();
    for (uint16_t i = 0; i < g_sample_count; i++) {
        JsonObject sample = samples.add<JsonObject>();
        sample["t_s"] = finite_or(g_samples[i].t_s, 0.0f);
        sample["weight_g"] = finite_or(g_samples[i].weight_g, 0.0f);
        sample["flow_g_per_s"] = finite_or(g_samples[i].flow_g_per_s, 0.0f);
    }

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.begin(webhook_url);
    http.addHeader("Content-Type", "application/json");
    int http_code = http.POST(body);
    if (http_code == HTTP_CODE_OK) {
        Serial.printf("[ShotLogger] Uploaded shot, HTTP %d\n", http_code);
        shot_upload_status_show(true, "Shot gesendet");
    } else if (http_code > 0) {
        // A response came back, just not the one that means HA accepted it -
        // a 4xx/5xx here used to be logged identically to success.
        Serial.printf("[ShotLogger] Upload rejected, HTTP %d\n", http_code);
        char message[48];
        snprintf(message, sizeof(message), "Shot-Upload fehlgeschlagen (HTTP %d)", http_code);
        shot_upload_status_show(false, message);
    } else {
        Serial.printf("[ShotLogger] Upload failed: %s (%d)\n",
                      HTTPClient::errorToString(http_code).c_str(), http_code);
        shot_upload_status_show(false, "Shot-Upload fehlgeschlagen");
    }
    http.end();
}
