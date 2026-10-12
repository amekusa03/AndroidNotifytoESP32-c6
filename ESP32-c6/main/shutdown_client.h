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

#ifdef __cplusplus
}
#endif
