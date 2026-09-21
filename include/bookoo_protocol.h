#pragma once

#include <stddef.h>
#include <stdint.h>

// Wire format of the BOOKOO Themis scales, as published by the manufacturer at
// https://github.com/BooKooCode/OpenSource. Deliberately free of Arduino types
// so the decoder can be exercised on a host compiler.
namespace bookoo {

// 16 bit UUIDs, expanded by the BLE stack to the Bluetooth base UUID.
constexpr uint16_t SERVICE_UUID   = 0x0FFE;
constexpr uint16_t WEIGHT_CHAR    = 0xFF11;  // notifications
constexpr uint16_t COMMAND_CHAR   = 0xFF12;  // writes

constexpr const char *NAME_PREFIX = "BOOKOO";

constexpr uint8_t PRODUCT_NUMBER  = 0x03;
constexpr uint8_t TYPE_COMMAND    = 0x0A;
constexpr uint8_t TYPE_WEIGHT     = 0x0B;
constexpr uint8_t TYPE_AUTO_MODE  = 0x0D;
constexpr uint8_t TYPE_POWDER     = 0x0F;

constexpr size_t NOTIFICATION_LENGTH = 20;
constexpr size_t COMMAND_LENGTH      = 6;

struct Reading {
    float   elapsed_s      = 0.0f;  // the scale's own timer
    float   weight_g       = 0.0f;
    float   flow_g_per_s   = 0.0f;  // as computed by the scale
    uint8_t battery_percent = 0;
    float   standby_minutes = 0.0f;
    uint8_t buzzer_level    = 0;
    bool    flow_smoothing  = false;
    bool    displays_ounces = false;  // the transported weight is always grams
};

enum class DecodeResult {
    Ok,
    Truncated,
    BadProduct,
    ChecksumMismatch,
    NotAWeightFrame,  // a valid frame, but carrying something else
};

// XOR over every byte but the last, which carries the checksum.
uint8_t checksum(const uint8_t *data, size_t len);

// Decodes a notification from the weight characteristic.
DecodeResult decode_weight(const uint8_t *data, size_t len, Reading &out);

enum class Command {
    Tare,
    StartTimer,
    StopTimer,
    ResetTimer,
    TareAndStartTimer,  // preferred over tare + start: the scale does both at once
    BeepLevel,          // 0 mutes, up to 5 on the Mini
    AutoOffMinutes,     // 5...30
    FlowSmoothing,      // value 0 or 1
};

// Writes COMMAND_LENGTH bytes into out. Returns false for an out of range value.
bool encode_command(Command command, int value, uint8_t out[COMMAND_LENGTH]);

const char *decode_result_name(DecodeResult result);

}  // namespace bookoo
