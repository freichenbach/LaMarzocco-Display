#include "bookoo_protocol.h"

namespace bookoo {
namespace {

// The scale sends the ASCII characters '+' and '-' as the sign.
float sign_of(uint8_t byte)
{
    return (byte == 0x2D) ? -1.0f : 1.0f;
}

uint32_t uint24(uint8_t high, uint8_t mid, uint8_t low)
{
    return ((uint32_t)high << 16) | ((uint32_t)mid << 8) | (uint32_t)low;
}

uint16_t uint16(uint8_t high, uint8_t low)
{
    return (uint16_t)(((uint16_t)high << 8) | (uint16_t)low);
}

}  // namespace

uint8_t checksum(const uint8_t *data, size_t len)
{
    uint8_t result = 0;
    for (size_t i = 0; i < len; i++) {
        result ^= data[i];
    }
    return result;
}

DecodeResult decode_weight(const uint8_t *data, size_t len, Reading &out)
{
    if (!data || len < NOTIFICATION_LENGTH) {
        return DecodeResult::Truncated;
    }
    if (data[0] != PRODUCT_NUMBER) {
        return DecodeResult::BadProduct;
    }
    if (checksum(data, NOTIFICATION_LENGTH - 1) != data[NOTIFICATION_LENGTH - 1]) {
        return DecodeResult::ChecksumMismatch;
    }
    if (data[1] != TYPE_WEIGHT) {
        return DecodeResult::NotAWeightFrame;
    }

    out.elapsed_s       = uint24(data[2], data[3], data[4]) / 1000.0f;
    out.displays_ounces = (data[5] == 0x02);
    out.weight_g        = sign_of(data[6]) * (uint24(data[7], data[8], data[9]) / 100.0f);
    out.flow_g_per_s    = sign_of(data[10]) * (uint16(data[11], data[12]) / 100.0f);
    out.battery_percent = data[13];
    out.standby_minutes = uint16(data[14], data[15]) / 10.0f;
    out.buzzer_level    = data[16];
    out.flow_smoothing  = (data[17] == 0x01);

    return DecodeResult::Ok;
}

bool encode_command(Command command, int value, uint8_t out[COMMAND_LENGTH])
{
    uint8_t payload[3] = {0, 0, 0};

    switch (command) {
        case Command::Tare:              payload[0] = 0x01; break;
        case Command::BeepLevel:
            if (value < 0 || value > 5) return false;
            payload[0] = 0x02;
            payload[2] = (uint8_t)value;
            break;
        case Command::AutoOffMinutes:
            if (value < 5 || value > 30) return false;
            payload[0] = 0x03;
            payload[2] = (uint8_t)value;
            break;
        case Command::StartTimer:        payload[0] = 0x04; break;
        case Command::StopTimer:         payload[0] = 0x05; break;
        case Command::ResetTimer:        payload[0] = 0x06; break;
        case Command::TareAndStartTimer: payload[0] = 0x07; break;
        case Command::FlowSmoothing:
            payload[0] = 0x08;
            payload[1] = value ? 0x01 : 0x00;
            break;
        default:
            return false;
    }

    out[0] = PRODUCT_NUMBER;
    out[1] = TYPE_COMMAND;
    out[2] = payload[0];
    out[3] = payload[1];
    out[4] = payload[2];
    out[5] = checksum(out, COMMAND_LENGTH - 1);
    return true;
}

const char *decode_result_name(DecodeResult result)
{
    switch (result) {
        case DecodeResult::Ok:               return "ok";
        case DecodeResult::Truncated:        return "truncated";
        case DecodeResult::BadProduct:       return "not a BOOKOO frame";
        case DecodeResult::ChecksumMismatch: return "checksum mismatch";
        case DecodeResult::NotAWeightFrame:  return "not a weight frame";
    }
    return "unknown";
}

}  // namespace bookoo
