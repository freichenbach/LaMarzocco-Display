// Host test for the BOOKOO frame decoder. The frames come from the vectors in
// LMControl's BookooProtocolTests.swift, which were in turn built from BooKoo's
// published protocol rather than from any implementation of it.
//
//   g++ -std=c++17 -I include test/bookoo/test_bookoo_protocol.cpp
//     src/bookoo_protocol.cpp -o /tmp/test_bookoo && /tmp/test_bookoo

#include "bookoo_protocol.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static int failures = 0;

static void check(bool condition, const char *what)
{
    if (condition) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        failures++;
    }
}

static void check_near(float actual, float expected, const char *what)
{
    bool ok = std::fabs(actual - expected) < 0.0001f;
    if (ok) {
        printf("  ok    %-28s = %.4f\n", what, actual);
    } else {
        printf("  FAIL  %-28s = %.4f, expected %.4f\n", what, actual, expected);
        failures++;
    }
}

int main()
{
    using namespace bookoo;

    printf("Weight notification\n");
    {
        const uint8_t frame[20] = {
            0x03, 0x0B, 0x00, 0x6A, 0x09, 0x01, 0x2B, 0x00, 0x0E, 0x3A,
            0x2B, 0x00, 0xEB, 0x51, 0x05, 0xDC, 0x03, 0x01, 0x00, 0x3F,
        };
        Reading r;
        check(decode_weight(frame, sizeof(frame), r) == DecodeResult::Ok, "decodes");
        check_near(r.elapsed_s, 27.145f, "elapsed_s");
        check_near(r.weight_g, 36.42f, "weight_g");
        check_near(r.flow_g_per_s, 2.35f, "flow_g_per_s");
        check(r.battery_percent == 81, "battery_percent == 81");
        check_near(r.standby_minutes, 150.0f, "standby_minutes");
        check(r.buzzer_level == 3, "buzzer_level == 3");
        check(r.flow_smoothing, "flow_smoothing");
        check(!r.displays_ounces, "displays grams");
    }

    printf("Negative weight\n");
    {
        const uint8_t frame[20] = {
            0x03, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x2D, 0x00, 0x00, 0x96,
            0x2B, 0x00, 0x00, 0x4D, 0x05, 0xDC, 0x03, 0x01, 0x00, 0x0F,
        };
        Reading r;
        check(decode_weight(frame, sizeof(frame), r) == DecodeResult::Ok, "decodes");
        check_near(r.weight_g, -1.5f, "weight_g");
        check(r.battery_percent == 77, "battery_percent == 77");
    }

    printf("Corrupted and short frames are rejected\n");
    {
        uint8_t frame[20] = {
            0x03, 0x0B, 0x00, 0x6A, 0x09, 0x01, 0x2B, 0x00, 0x0E, 0x3A,
            0x2B, 0x00, 0xEB, 0x51, 0x05, 0xDC, 0x03, 0x01, 0x00, 0x3F,
        };
        frame[8] = 0x0F;  // a flipped weight byte must not slip through
        Reading r;
        check(decode_weight(frame, sizeof(frame), r) == DecodeResult::ChecksumMismatch,
              "flipped byte caught by the checksum");

        const uint8_t truncated[3] = {0x03, 0x0B, 0x00};
        check(decode_weight(truncated, sizeof(truncated), r) == DecodeResult::Truncated,
              "short frame rejected");

        uint8_t powder[20] = {0x03, 0x0F, 0x2B, 0x00, 0x07, 0x3A};
        powder[19] = checksum(powder, 19);
        check(decode_weight(powder, sizeof(powder), r) == DecodeResult::NotAWeightFrame,
              "powder frame is not mistaken for a weight");
    }

    printf("Command encoding\n");
    {
        struct Case {
            Command command;
            int value;
            uint8_t expected[6];
            const char *name;
        } cases[] = {
            {Command::Tare,              0,  {0x03, 0x0A, 0x01, 0x00, 0x00, 0x08}, "tare"},
            {Command::TareAndStartTimer, 0,  {0x03, 0x0A, 0x07, 0x00, 0x00, 0x0E}, "tare and start"},
            {Command::StartTimer,        0,  {0x03, 0x0A, 0x04, 0x00, 0x00, 0x0D}, "start timer"},
            {Command::StopTimer,         0,  {0x03, 0x0A, 0x05, 0x00, 0x00, 0x0C}, "stop timer"},
            {Command::ResetTimer,        0,  {0x03, 0x0A, 0x06, 0x00, 0x00, 0x0F}, "reset timer"},
            {Command::BeepLevel,         3,  {0x03, 0x0A, 0x02, 0x00, 0x03, 0x08}, "beep level 3"},
            {Command::AutoOffMinutes,    20, {0x03, 0x0A, 0x03, 0x00, 0x14, 0x1E}, "auto off 20 min"},
            {Command::FlowSmoothing,     1,  {0x03, 0x0A, 0x08, 0x01, 0x00, 0x00}, "flow smoothing on"},
        };

        for (const auto &c : cases) {
            uint8_t out[6] = {0};
            bool encoded = encode_command(c.command, c.value, out);
            check(encoded && memcmp(out, c.expected, 6) == 0, c.name);
        }

        uint8_t out[6];
        check(!encode_command(Command::BeepLevel, 9, out), "beep level 9 rejected");
        check(!encode_command(Command::AutoOffMinutes, 2, out), "auto off 2 min rejected");
        check(!encode_command(Command::AutoOffMinutes, 45, out), "auto off 45 min rejected");
    }

    printf("\n%s\n", failures == 0 ? "All checks passed." : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
