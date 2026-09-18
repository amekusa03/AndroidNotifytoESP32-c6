#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the pc control client.
 */
esp_err_t shutdown_client_init(void);

/**
 * @brief Send a generic token to the target PC and receive response.
 * @param token The token to send
 * @param rx_buf Optional buffer to store received response (can be NULL)
 * @param rx_len Size of rx_buf
 */
esp_err_t shutdown_client_send_token_ext(const char *token, char *rx_buf, size_t rx_len);

/**
 * @brief Send a generic token to the target PC.
 */
esp_err_t shutdown_client_send_token(const char *token);

/**
 * @brief Send the shutdown trigger to the target PC.
 */
esp_err_t shutdown_client_execute(void);

/**
 * @brief Send Volume Up token to target PC.
 */
esp_err_t shutdown_client_volume_up(void);

/**
 * @brief Send Volume Down token to target PC.
 */
esp_err_t shutdown_client_volume_down(void);

/**
 * @brief Send Volume Mute toggle token to target PC.
 */
esp_err_t shutdown_client_volume_mute(void);

/**
 * @brief Send Volume Set token to target PC.
 * @param volume_pct Volume percentage (0-100)
 */
esp_err_t shutdown_client_set_volume(uint8_t volume_pct);

/**
 * @brief Get current PC volume percentage and mute status.
 * @param[out] out_volume Pointer to store volume percentage (0-100)
 * @param[out] out_muted Pointer to store mute status (true if muted)
 */
esp_err_t shutdown_client_get_volume(int *out_volume, bool *out_muted);

#ifdef __cplusplus
}
#endif
