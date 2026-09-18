#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECTED
} lcd_wifi_status_t;

typedef enum {
    PC_STATUS_OFF,
    PC_STATUS_ON,
    PC_STATUS_BOOTING
} lcd_pc_status_t;

/**
 * @brief Initialize the Waveshare ESP32-C6-GEEK LCD display (ST7789 240x135) and backlight.
 */
esp_err_t lcd_display_init(void);

/**
 * @brief Update Wi-Fi status and displayed IP address.
 */
void lcd_display_update_wifi(lcd_wifi_status_t status, const char *ip_str);

/**
 * @brief Update PC power status (ON / OFF / BOOTING).
 */
void lcd_display_update_pc(lcd_pc_status_t status);

/**
 * @brief Set custom message line on the LCD display.
 */
void lcd_display_set_message(const char *msg);

/**
 * @brief Enable or disable the LCD backlight.
 */
void lcd_display_set_backlight(bool enable);

/**
 * @brief Toggle the LCD backlight state.
 * @return True if backlight is now enabled, false otherwise.
 */
bool lcd_display_toggle_backlight(void);

/**
 * @brief Show notification image on LCD screen.
 * @param raw_rgb565 Raw RGB565 pixel buffer (240x135x2 bytes, big endian).
 * @param duration_ms Duration in milliseconds to display notification before returning to status screen.
 */
void lcd_display_show_notification(const uint8_t *raw_rgb565, uint32_t duration_ms);

/**
 * @brief Check if a notification is currently active on screen.
 */
bool lcd_display_is_notification_active(void);

/**
 * @brief Dismiss active notification and return immediately to normal status screen.
 */
void lcd_display_dismiss_notification(void);

#ifdef __cplusplus
}
#endif

#endif // LCD_DISPLAY_H
