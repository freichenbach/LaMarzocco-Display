#include "scale_ble.h"
#include "config.h"
#include "wifi_power.h"
#include "shot_log.h"

#include <Arduino.h>

#if SCALE_BLE_ENABLED

#include <NimBLEDevice.h>

namespace {

// The notification callback runs on the NimBLE host task, so the reading is
// published under a spinlock rather than handed straight to the display.
portMUX_TYPE g_reading_lock = portMUX_INITIALIZER_UNLOCKED;
bookoo::Reading g_reading;
bool g_have_reading = false;
uint32_t g_reading_ms = 0;

NimBLEClient *g_client = nullptr;
NimBLERemoteCharacteristic *g_command_char = nullptr;
NimBLEAddress g_found_address;
volatile bool g_found_scale = false;
volatile bool g_connected = false;
unsigned long g_last_scan_ms = 0;

void on_notify(NimBLERemoteCharacteristic *, uint8_t *data, size_t length, bool)
{
    bookoo::Reading reading;
    bookoo::DecodeResult result = bookoo::decode_weight(data, length, reading);
    if (result != bookoo::DecodeResult::Ok) {
        // Anything but a weight frame is normal traffic, not an error worth
        // reporting on every packet.
        if (result == bookoo::DecodeResult::ChecksumMismatch) {
            Serial.println("[SCALE] Discarded a frame with a bad checksum");
        }
        return;
    }

    portENTER_CRITICAL(&g_reading_lock);
    g_reading = reading;
    g_have_reading = true;
    g_reading_ms = millis();
    portEXIT_CRITICAL(&g_reading_lock);
}

class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice *device) override
    {
        if (g_found_scale) {
            return;
        }
        std::string name = device->getName();
        if (name.rfind(bookoo::NAME_PREFIX, 0) != 0) {
            return;
        }
        Serial.printf("[SCALE] Found '%s' at %s, %d dBm\n", name.c_str(),
                      device->getAddress().toString().c_str(), device->getRSSI());
        g_found_address = device->getAddress();
        g_found_scale = true;
        NimBLEDevice::getScan()->stop();
    }
};

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient *) override
    {
        Serial.println("[SCALE] Connected");
    }
    void onDisconnect(NimBLEClient *) override
    {
        Serial.println("[SCALE] Disconnected");
        g_connected = false;
        g_command_char = nullptr;
    }
};

ScanCallbacks g_scan_callbacks;
ClientCallbacks g_client_callbacks;

void start_scan()
{
    g_last_scan_ms = millis();
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&g_scan_callbacks, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(50);  // half the interval: leaves radio time for WiFi
    scan->start(SCALE_SCAN_SECONDS, nullptr, false);
}

bool connect_to_scale()
{
    if (!g_client) {
        g_client = NimBLEDevice::createClient();
        g_client->setClientCallbacks(&g_client_callbacks, false);
        g_client->setConnectTimeout(10);
    }

    if (!g_client->connect(g_found_address)) {
        Serial.println("[SCALE] Connection failed");
        return false;
    }

    NimBLERemoteService *service = g_client->getService(NimBLEUUID(bookoo::SERVICE_UUID));
    if (!service) {
        Serial.println("[SCALE] Scale has no weight service - wrong device?");
        g_client->disconnect();
        return false;
    }

    NimBLERemoteCharacteristic *weight = service->getCharacteristic(NimBLEUUID(bookoo::WEIGHT_CHAR));
    if (!weight || !weight->canNotify() || !weight->subscribe(true, on_notify)) {
        Serial.println("[SCALE] Could not subscribe to weight notifications");
        g_client->disconnect();
        return false;
    }

    g_command_char = service->getCharacteristic(NimBLEUUID(bookoo::COMMAND_CHAR));
    g_connected = true;
    Serial.println("[SCALE] Subscribed to weight notifications");
    shot_log_printf("[SCALE] connected and subscribed");
    return true;
}

}  // namespace

void scale_ble_begin(void)
{
    Serial.println("[SCALE] Starting Bluetooth for the scale");
    // WiFi and Bluetooth share one radio, and the software that interleaves
    // them refuses to start while WiFi is told never to sleep - enabling the
    // Bluetooth controller then aborts in coex_core_enable and the board
    // reboots. Settle the power save mode here, right before the controller
    // comes up, whoever set it last.
    wifi_apply_power_save();
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    start_scan();
}

void scale_ble_loop(void)
{
    if (g_connected) {
        return;
    }

    if (g_found_scale) {
        g_found_scale = false;
        if (!connect_to_scale()) {
            g_last_scan_ms = millis();  // back off before scanning again
        }
        return;
    }

    if (millis() - g_last_scan_ms >= SCALE_SCAN_INTERVAL_MS) {
        start_scan();
    }
}

bool scale_ble_is_connected(void)
{
    return g_connected;
}

bool scale_ble_last_reading(bookoo::Reading &out, uint32_t &age_ms)
{
    bool have;
    portENTER_CRITICAL(&g_reading_lock);
    have = g_have_reading;
    out = g_reading;
    uint32_t stamp = g_reading_ms;
    portEXIT_CRITICAL(&g_reading_lock);

    age_ms = have ? (millis() - stamp) : 0;
    return have;
}

bool scale_ble_send(bookoo::Command command, int value)
{
    if (!g_connected || !g_command_char) {
        return false;
    }
    uint8_t frame[bookoo::COMMAND_LENGTH];
    if (!bookoo::encode_command(command, value, frame)) {
        return false;
    }
    return g_command_char->writeValue(frame, sizeof(frame), false);
}

#else  // SCALE_BLE_ENABLED

void scale_ble_begin(void) {}
void scale_ble_loop(void) {}
bool scale_ble_is_connected(void) { return false; }
bool scale_ble_last_reading(bookoo::Reading &, uint32_t &) { return false; }
bool scale_ble_send(bookoo::Command, int) { return false; }

#endif
