#pragma once
#include <stdbool.h>
#include <esp_err.h>
#include "pc_control.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback invoked when state mismatch is confirmed. actual_on = actual state from USB connection.
// Updates Matter attribute in callback to notify user.
// The system must NOT perform ON/OFF operations inside this callback (fail-safe principle).
typedef void (*pc_monitor_mismatch_cb_t)(bool actual_on);

// Callback invoked on PC power state transition (e.g. OFF -> ON)
typedef void (*pc_monitor_state_change_cb_t)(pc_power_state_t new_state);

// Call after pc_control_init().
esp_err_t pc_monitor_init(pc_monitor_mismatch_cb_t mismatch_cb, pc_monitor_state_change_cb_t state_cb);

// Call upon executing a Matter command.
// want_on = expected command state. Suppresses transient state detection during boot/shutdown.
void pc_monitor_notify_command(bool want_on);

#ifdef __cplusplus
}
#endif
