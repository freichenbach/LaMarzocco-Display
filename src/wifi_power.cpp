#include "wifi_power.h"

#include <WiFi.h>

#include "config.h"

void wifi_apply_power_save(void)
{
#if SCALE_BLE_ENABLED
    // Modem sleep still keeps the connection up: the radio wakes for every
    // beacon carrying traffic for us. It costs a little latency and buys
    // coexistence with Bluetooth, which is not optional - without it the
    // Bluetooth controller aborts the moment it is enabled.
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
#else
    WiFi.setSleep(false);
#endif
}
