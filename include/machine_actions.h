#pragma once

// The UI event callbacks run inside the LVGL task. Talking to the cloud from
// there is not safe: a TLS handshake needs far more stack than that task has,
// and it would use the same client the main loop is already using, so both end
// up inside mbedTLS on one context. The callbacks therefore only record what
// the user asked for, and the main loop carries it out.
void machine_action_request_power_toggle(void);
void machine_action_request_steam_toggle(void);

// Call from loop(): performs a pending action, if there is one.
void machine_actions_process(void);
