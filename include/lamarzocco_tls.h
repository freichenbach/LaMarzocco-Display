#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <time.h>

// Anything below this is not a real date but an unset clock: the ESP32 starts
// counting at 1970 and NTP has not answered yet. 2023-11-14, well before any
// build of this firmware.
static const time_t LM_MIN_VALID_EPOCH = 1700000000;

// Trust store used for all TLS connections to the La Marzocco cloud
// (REST API and WebSocket). See src/lamarzocco_tls.cpp for the contents.
extern const char LM_TLS_ROOT_CA_STORE[];

// Applies the trust store to a WiFiClientSecure. When the build defines
// LM_TLS_INSECURE, certificate verification is disabled instead.
void lm_tls_apply(WiFiClientSecure& client);

// Returns the trust store for WebSocketsClient::beginSslWithCA(), or nullptr
// when the build disables verification (LM_TLS_INSECURE).
const char* lm_tls_ca_store();

// Blocks until the system clock has been set by NTP, which is required before
// certificate validity dates can be checked. Returns false on timeout.
bool lm_tls_wait_for_clock(uint32_t timeout_ms);
