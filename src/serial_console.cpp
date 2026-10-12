#include "serial_console.h"

#include <Arduino.h>
#include <Preferences.h>

extern Preferences preferences;

namespace {

constexpr size_t MAX_LINE_LENGTH = 256;
constexpr size_t MAX_KEY_LENGTH = 15;  // ESP32 NVS key limit

String g_line;

void print_usage()
{
    Serial.println("[Console] Commands:");
    Serial.println("[Console]   set <KEY> <value>   e.g. set HA_WEBHOOK http://homeassistant.local:8123/api/webhook/...");
    Serial.println("[Console]   get <KEY>            e.g. get HA_WEBHOOK");
}

bool valid_key(const String &key)
{
    if (key.length() == 0 || key.length() > MAX_KEY_LENGTH) {
        Serial.printf("[Console] Key must be 1-%u characters\n", (unsigned)MAX_KEY_LENGTH);
        return false;
    }
    return true;
}

void handle_set(const String &rest)
{
    int space = rest.indexOf(' ');
    if (space < 0) {
        Serial.println("[Console] Usage: set <KEY> <value>");
        return;
    }
    String key = rest.substring(0, space);
    String value = rest.substring(space + 1);
    key.trim();
    if (!valid_key(key)) {
        return;
    }
    preferences.putString(key.c_str(), value);
    Serial.printf("[Console] %s = %s\n", key.c_str(), value.c_str());
}

void handle_get(const String &rest)
{
    String key = rest;
    key.trim();
    if (!valid_key(key)) {
        return;
    }
    String value = preferences.getString(key.c_str(), "");
    if (value.length() == 0) {
        Serial.printf("[Console] %s is not set\n", key.c_str());
    } else {
        Serial.printf("[Console] %s = %s\n", key.c_str(), value.c_str());
    }
}

void handle_line(String line)
{
    line.trim();
    if (line.length() == 0) {
        return;
    }
    if (line.startsWith("set ")) {
        handle_set(line.substring(4));
    } else if (line.startsWith("get ")) {
        handle_get(line.substring(4));
    } else {
        print_usage();
    }
}

}  // namespace

void serial_console_loop(void)
{
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (g_line.length() > 0) {
                handle_line(g_line);
                g_line = "";
            }
            continue;
        }
        if (g_line.length() < MAX_LINE_LENGTH) {
            g_line += c;
        }
    }
}
