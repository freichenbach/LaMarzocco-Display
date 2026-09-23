#include "shot_logger.h"
#include "config.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

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
    return "boot" + String(millis());
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
        sample["t_s"] = (float)(i * SHOT_SAMPLE_INTERVAL_MS) / 1000.0f;
        sample["weight_g"] = g_samples[i].weight_g;
        sample["flow_g_per_s"] = g_samples[i].flow_g_per_s;
    }

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.begin(webhook_url);
    http.addHeader("Content-Type", "application/json");
    int http_code = http.POST(body);
    if (http_code > 0) {
        Serial.printf("[ShotLogger] Uploaded shot, HTTP %d\n", http_code);
    } else {
        Serial.printf("[ShotLogger] Upload failed: %s (%d)\n",
                      HTTPClient::errorToString(http_code).c_str(), http_code);
    }
    http.end();
}
