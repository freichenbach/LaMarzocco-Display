#include "shot_log.h"

#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_system.h>
#include <stdarg.h>

#include "config.h"

namespace {

const char *LOG_PATH = "/shotlog.txt";

// Enough for a couple of shots plus their surroundings. Past this the file is
// started over: the last session is worth more than the first one.
const size_t LOG_MAX_BYTES = 24 * 1024;

// Holds what has not reached flash yet. One shot writes a line every half
// second, and loop() empties this every few milliseconds, so it only has to
// cover a burst.
const size_t BUFFER_BYTES = 2048;

char g_buffer[BUFFER_BYTES];
volatile size_t g_buffer_used = 0;
portMUX_TYPE g_buffer_lock = portMUX_INITIALIZER_UNLOCKED;

bool g_ready = false;

const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON:  return "power on";
        case ESP_RST_EXT:      return "reset pin";
        case ESP_RST_SW:       return "software restart";
        case ESP_RST_PANIC:    return "PANIC - crashed";
        case ESP_RST_INT_WDT:  return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT:      return "watchdog";
        case ESP_RST_BROWNOUT: return "BROWNOUT - supply dipped";
        case ESP_RST_SDIO:     return "SDIO";
        case ESP_RST_DEEPSLEEP: return "woke from deep sleep";
        default:               return "unknown";
    }
}

}  // namespace

void shot_log_begin(void)
{
    // Never format on failure: this filesystem also holds the setup portal's
    // pages, and losing those to make room for a log is a bad trade.
    if (!SPIFFS.begin()) {
        Serial.println("[LOG] No filesystem - the shot log will not be kept");
        return;
    }
    g_ready = true;

    // Whatever the last session recorded, before this one overwrites anything.
    shot_log_dump_and_clear();

    esp_reset_reason_t reason = esp_reset_reason();
    shot_log_printf("[BOOT] %s, heap %u", reset_reason_name(reason),
                    (unsigned)ESP.getFreeHeap());
    if (reason == ESP_RST_PANIC || reason == ESP_RST_BROWNOUT ||
        reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT) {
        // The same line on serial, because this is the one a reader must not
        // miss: the previous run did not end on its own terms.
        Serial.printf("[LOG] Previous run ended in: %s\n", reset_reason_name(reason));
    }
    shot_log_flush();
}

void shot_log_printf(const char *fmt, ...)
{
    char line[160];
    unsigned long now = millis();
    int head = snprintf(line, sizeof(line), "%lu.%03lu ", now / 1000, now % 1000);
    if (head < 0 || (size_t)head >= sizeof(line)) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int body = vsnprintf(line + head, sizeof(line) - head - 1, fmt, args);
    va_end(args);
    if (body < 0) {
        return;
    }

    size_t len = strlen(line);
    if (len + 1 < sizeof(line)) {
        line[len++] = '\n';
        line[len] = '\0';
    }

    portENTER_CRITICAL(&g_buffer_lock);
    if (g_buffer_used + len <= BUFFER_BYTES) {
        memcpy(g_buffer + g_buffer_used, line, len);
        g_buffer_used += len;
    }
    portEXIT_CRITICAL(&g_buffer_lock);
}

void shot_log_flush(void)
{
    if (!g_ready || g_buffer_used == 0) {
        return;
    }

    char pending[BUFFER_BYTES];
    size_t len;
    portENTER_CRITICAL(&g_buffer_lock);
    len = g_buffer_used;
    memcpy(pending, g_buffer, len);
    g_buffer_used = 0;
    portEXIT_CRITICAL(&g_buffer_lock);

    File file = SPIFFS.open(LOG_PATH, FILE_APPEND);
    if (!file) {
        return;
    }
    if (file.size() > LOG_MAX_BYTES) {
        file.close();
        SPIFFS.remove(LOG_PATH);
        file = SPIFFS.open(LOG_PATH, FILE_APPEND);
        if (!file) {
            return;
        }
        file.print("--- log restarted, it had grown too large ---\n");
    }
    file.write((const uint8_t *)pending, len);
    file.close();  // Closing is the flush: a crash keeps everything up to here.
}

void shot_log_dump_and_clear(void)
{
    if (!g_ready) {
        return;
    }

    File file = SPIFFS.open(LOG_PATH, FILE_READ);
    if (!file || file.size() == 0) {
        if (file) file.close();
        return;
    }

    Serial.println("\n===== log from the previous run =====");
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println("===== end of the previous run =====\n");
    file.close();
    SPIFFS.remove(LOG_PATH);
}
