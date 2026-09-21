#ifndef WIFI_POWER_H
#define WIFI_POWER_H

// WiFi power saving has to agree with Bluetooth. The radio is shared, and the
// software that interleaves the two refuses to start while WiFi is told never
// to sleep: enabling the Bluetooth controller then aborts in coex_core_enable
// and the board reboots in a loop. Every place that touches the power save
// mode goes through here so the two stay consistent.
void wifi_apply_power_save(void);

#endif  // WIFI_POWER_H
