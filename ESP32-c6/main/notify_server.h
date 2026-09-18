#ifndef NOTIFY_SERVER_H
#define NOTIFY_SERVER_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the TCP Notification Server task and mDNS responder on ESP32-C6.
 *
 * Listens on NOTIFY_TCP_PORT (default 5555) for incoming 240x135 RGB565 notification images
 * and updates the LCD display automatically.
 *
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t notify_server_init(void);

/**
 * @brief Initialize or update mDNS responder with hostname and notify service.
 *
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t notify_server_init_mdns(void);

#ifdef __cplusplus
}
#endif

#endif // NOTIFY_SERVER_H
