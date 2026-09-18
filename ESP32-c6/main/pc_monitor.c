#include "pc_monitor.h"
#include "pc_control.h"
#include "lcd_display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "pc_monitor";

// -----------------------------------------------------------------------
// State Mismatch Detection Design (USB Communication Based)
//
// Compares actual PC state (USB Serial/JTAG connection) with Matter attribute (expected state).
// When mismatch is confirmed, updates Matter attribute via callback and delegates decision to user.
// -----------------------------------------------------------------------

#define POLL_INTERVAL_MS    2000   // USB status polling interval
#define TRANSITION_WAIT_MS  1000   // Additional wait time when TRANSITIONING is detected
#define MISMATCH_THRESHOLD     3   // Confirmed after 3 consecutive mismatches
#define INHIBIT_MS         30000   // Monitoring inhibit duration after command (considering boot time)

static pc_monitor_mismatch_cb_t     s_mismatch_cb   = NULL;
static pc_monitor_state_change_cb_t s_state_cb      = NULL;
static volatile bool                 s_expected_on   = false;
static volatile TickType_t           s_inhibit_until = 0;
static volatile int                  s_mismatch_count = 0;

void pc_monitor_notify_command(bool want_on)
{
    s_expected_on    = want_on;
    s_inhibit_until  = xTaskGetTickCount() + pdMS_TO_TICKS(INHIBIT_MS);
    s_mismatch_count = 0;
    ESP_LOGI(TAG, "Command: %s, inhibit %dms", want_on ? "ON" : "OFF", INHIBIT_MS);
    if (want_on) {
        lcd_display_update_pc(PC_STATUS_BOOTING);
        lcd_display_set_message("AnyKey Sent");
    }
}

extern void app_update_wifi_power_save(bool pc_on);

static void monitor_task(void *arg)
{
    static pc_power_state_t s_last_state = PC_STATE_TRANSITIONING;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));

        pc_power_state_t state = pc_get_power_state();

        if (state != s_last_state) {
            pc_power_state_t prev = s_last_state;
            s_last_state = state;
            if (state == PC_STATE_ON || state == PC_STATE_OFF) {
                app_update_wifi_power_save(state == PC_STATE_ON);
            }

            if (prev != PC_STATE_ON && state == PC_STATE_ON) {
                ESP_LOGI(TAG, "PC state transition detected: OFF/TRANSITIONING -> ON");
                if (s_state_cb) {
                    s_state_cb(PC_STATE_ON);
                }
            }
        }

        if (state == PC_STATE_ON) {
            lcd_display_update_pc(PC_STATUS_ON);
        } else if (state == PC_STATE_OFF) {
            lcd_display_update_pc(PC_STATUS_OFF);
        } else if (state == PC_STATE_TRANSITIONING) {
            lcd_display_update_pc(PC_STATUS_BOOTING);
        }

        // Do not detect during transient period after command
        if (xTaskGetTickCount() < s_inhibit_until) {
            s_mismatch_count = 0;
            continue;
        }

        if (state == PC_STATE_TRANSITIONING) {
            vTaskDelay(pdMS_TO_TICKS(TRANSITION_WAIT_MS));
            s_mismatch_count = 0;
            continue;
        }

        bool actual_on = (state == PC_STATE_ON);

        if (actual_on == s_expected_on) {
            s_mismatch_count = 0;
            continue;
        }

        // Mismatch detected: wait until threshold is reached
        if (++s_mismatch_count < MISMATCH_THRESHOLD) {
            ESP_LOGW(TAG, "Mismatch %d/%d: expected=%s actual=%s",
                     s_mismatch_count, MISMATCH_THRESHOLD,
                     s_expected_on ? "ON" : "OFF", actual_on ? "ON" : "OFF");
            continue;
        }

        // Mismatch confirmed
        s_mismatch_count = 0;
        ESP_LOGE(TAG, "USB status mismatch confirmed: expected=%s actual=%s -> syncing Matter attribute",
                 s_expected_on ? "ON" : "OFF", actual_on ? "ON" : "OFF");
        s_expected_on = actual_on;

        if (s_mismatch_cb) {
            s_mismatch_cb(actual_on);
        }
    }
}

esp_err_t pc_monitor_init(pc_monitor_mismatch_cb_t mismatch_cb, pc_monitor_state_change_cb_t state_cb)
{
    s_mismatch_cb = mismatch_cb;
    s_state_cb    = state_cb;
    s_expected_on = (pc_get_power_state() == PC_STATE_ON);
    xTaskCreate(monitor_task, "pc_monitor", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Started (USB communication monitoring, poll=%dms, inhibit=%dms, threshold=%d)",
             POLL_INTERVAL_MS, INHIBIT_MS, MISMATCH_THRESHOLD);
    return ESP_OK;
}
