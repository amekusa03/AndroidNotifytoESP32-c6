#include "shutdown_client.h"
#include "wifi_creds.h"
#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>
#include <errno.h>
#include <mdns.h>

static const char *TAG = "pc_control_client";

esp_err_t shutdown_client_init(void)
{
    ESP_LOGI(TAG, "PC control client initialized");
    return ESP_OK;
}

esp_err_t shutdown_client_send_token_ext(const char *token, char *rx_buf, size_t rx_len)
{
    if (!token) return ESP_ERR_INVALID_ARG;

    char ip_str[16] = {0};

#ifdef PC_STATIC_IP
    strncpy(ip_str, PC_STATIC_IP, sizeof(ip_str) - 1);
    ESP_LOGI(TAG, "Using static IP: %s", ip_str);
#else
    esp_ip4_addr_t addr;
    addr.addr = 0;
    ESP_LOGI(TAG, "Resolving PC hostname '%s' via mDNS...", PC_HOSTNAME);
    esp_err_t mdns_err = mdns_query_a(PC_HOSTNAME, 2000, &addr);
    if (mdns_err == ESP_OK && addr.addr != 0) {
        esp_ip4addr_ntoa(&addr, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "mDNS resolved %s.local -> %s", PC_HOSTNAME, ip_str);
    } else {
        ESP_LOGW(TAG, "mDNS query for '%s' returned %d, falling back to getaddrinfo...", PC_HOSTNAME, mdns_err);
        char hostname_buf[64];
        snprintf(hostname_buf, sizeof(hostname_buf), "%s.local", PC_HOSTNAME);
        struct addrinfo hints = {
            .ai_family   = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };
        struct addrinfo *res = NULL;
        int gai_err = getaddrinfo(hostname_buf, NULL, &hints, &res);
        if (gai_err != 0 || res == NULL) {
            ESP_LOGE(TAG, "getaddrinfo failed for %s: %d", hostname_buf, gai_err);
            if (res) freeaddrinfo(res);
            return ESP_FAIL;
        }
        struct in_addr resolved_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        snprintf(ip_str, sizeof(ip_str), "%s", inet_ntoa(resolved_addr));
        freeaddrinfo(res);
        ESP_LOGI(TAG, "Resolved %s -> %s", hostname_buf, ip_str);
    }
#endif

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_addr.s_addr = inet_addr(ip_str);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PC_CONTROL_PORT);

    ESP_LOGI(TAG, "Connecting to %s:%d...", ip_str, PC_CONTROL_PORT);
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG, "Connection failed: errno %d", errno);
        close(sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending token: %s", token);
    int err = send(sock, token, strlen(token), 0);
    if (err < 0) {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
        close(sock);
        return ESP_FAIL;
    }

    char local_buf[128];
    int len = recv(sock, local_buf, sizeof(local_buf) - 1, 0);
    if (len > 0) {
        local_buf[len] = '\0';
        ESP_LOGI(TAG, "Received response: %s", local_buf);
        if (rx_buf && rx_len > 0) {
            strncpy(rx_buf, local_buf, rx_len - 1);
            rx_buf[rx_len - 1] = '\0';
        }
    } else if (rx_buf && rx_len > 0) {
        rx_buf[0] = '\0';
    }

    close(sock);
    ESP_LOGI(TAG, "Token sent successfully");
    return ESP_OK;
}

esp_err_t shutdown_client_send_token(const char *token)
{
    return shutdown_client_send_token_ext(token, NULL, 0);
}

esp_err_t shutdown_client_execute(void)
{
    return shutdown_client_send_token(PC_CONTROL_TOKEN_SHUTDOWN);
}

