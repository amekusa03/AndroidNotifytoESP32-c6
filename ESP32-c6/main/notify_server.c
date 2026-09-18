#include "notify_server.h"
#include "lcd_display.h"
#include "wifi_creds.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_netif.h>
#include <mdns.h>

static const char *TAG = "notify_server";

#ifndef NOTIFY_TCP_PORT
#define NOTIFY_TCP_PORT 5555
#endif

#define NOTIFY_IMG_WIDTH  240
#define NOTIFY_IMG_HEIGHT 135
#define NOTIFY_RAW_SIZE   (NOTIFY_IMG_WIDTH * NOTIFY_IMG_HEIGHT * 2) // 64,800 bytes (LE RGB565)
#define NOTIFY_NT_SIZE    (NOTIFY_RAW_SIZE + 4)                      // NT header + pixel data = 64,804 bytes
#define NOTIFY_BUF_SIZE   (NOTIFY_NT_SIZE)                           // Allocate for exactly one NT packet

// Track whether the delegated hostname has been registered
static bool s_delegate_registered = false;

esp_err_t notify_server_init_mdns(void) {
    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "mdns_init failed: %d", err);
    }

    // Fetch current STA IP to associate with the delegated hostname
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif || esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
        ESP_LOGW(TAG, "STA IP not ready yet, skipping mDNS delegate registration");
        return ESP_FAIL;
    }

    mdns_ip_addr_t addr4 = {};
    addr4.addr.type = ESP_IPADDR_TYPE_V4;
    addr4.addr.u_addr.ip4.addr = ip_info.ip.addr;
    addr4.next = NULL;

    if (!s_delegate_registered) {
        // First registration: add delegated hostname esp32-notify.local
        err = mdns_delegate_hostname_add(MDNS_HOSTNAME, &addr4);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "mDNS delegated host added: %s.local -> " IPSTR,
                     MDNS_HOSTNAME, IP2STR(&ip_info.ip));
            s_delegate_registered = true;
        } else {
            ESP_LOGW(TAG, "mdns_delegate_hostname_add failed: %d", err);
            return err;
        }

        // Register _notify._tcp service under esp32-notify.local
        mdns_txt_item_t txt[] = {
            {"board",  "esp32-c6-geek"},
            {"width",  "240"},
            {"height", "135"},
            {"format", "RGB565"}
        };
        err = mdns_service_add_for_host(MDNS_INSTANCE_NAME, "_notify", "_tcp",
                                        MDNS_HOSTNAME, NOTIFY_TCP_PORT,
                                        txt, sizeof(txt)/sizeof(txt[0]));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "mDNS service registered: %s._notify._tcp.local:%d",
                     MDNS_HOSTNAME, NOTIFY_TCP_PORT);
        } else {
            ESP_LOGW(TAG, "mdns_service_add_for_host failed: %d", err);
        }
    } else {
        // IP may have changed — update address of existing delegated hostname
        err = mdns_delegate_hostname_set_address(MDNS_HOSTNAME, &addr4);
        ESP_LOGI(TAG, "mDNS delegated host address updated: %s.local -> " IPSTR " (%d)",
                 MDNS_HOSTNAME, IP2STR(&ip_info.ip), err);
    }

    return ESP_OK;
}

static void notify_server_task(void *pvParam) {
    uint8_t *rx_buf = (uint8_t *)heap_caps_malloc(NOTIFY_BUF_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!rx_buf) {
        rx_buf = (uint8_t *)malloc(NOTIFY_BUF_SIZE);
    }
    if (!rx_buf) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for notification rx buffer", NOTIFY_BUF_SIZE);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port        = htons(NOTIFY_TCP_PORT);

    int lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (lsock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        free(rx_buf);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(lsock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket on port %d: errno %d", NOTIFY_TCP_PORT, errno);
        close(lsock);
        free(rx_buf);
        vTaskDelete(NULL);
        return;
    }

    if (listen(lsock, 2) < 0) {
        ESP_LOGE(TAG, "Failed to listen on socket: errno %d", errno);
        close(lsock);
        free(rx_buf);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Notification TCP Server listening on port %d", NOTIFY_TCP_PORT);

    while (1) {
        struct sockaddr_in client_addr = {};
        socklen_t client_len = sizeof(client_addr);
        int sock = accept(lsock, (struct sockaddr *)&client_addr, &client_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Accept error: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = {0};
        inet_ntoa_r(client_addr.sin_addr, client_ip, sizeof(client_ip));

        // Set socket receive timeout (5 seconds)
        struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Read first 4 bytes to detect header
        uint8_t header[4] = {0};
        int hlen = 0;
        while (hlen < 4) {
            int n = recv(sock, header + hlen, 4 - hlen, 0);
            if (n <= 0) break;
            hlen += n;
        }

        if (hlen < 4) {
            ESP_LOGW(TAG, "Failed to read header (%d bytes)", hlen);
            close(sock);
            continue;
        }

        bool has_nt_header = (header[0] == 'N' && header[1] == 'T');
        uint32_t duration_ms = NOTIFY_DISPLAY_TIMEOUT_MS;

        if (has_nt_header) {
            uint16_t duration_sec = ((uint16_t)header[2] << 8) | header[3];
            if (duration_sec > 0 && duration_sec <= 600) {
                duration_ms = (uint32_t)duration_sec * 1000;
            }
            ESP_LOGI(TAG, "'NT' header from %s: duration=%lu ms", client_ip, (unsigned long)duration_ms);
        } else {
            // No NT header - first 4 bytes are pixel data; copy them to rx_buf start
            memcpy(rx_buf, header, 4);
        }

        // Read pixel data (exactly NOTIFY_RAW_SIZE bytes)
        size_t offset = has_nt_header ? 0 : 4;
        size_t remaining = NOTIFY_RAW_SIZE - offset;

        while (offset < NOTIFY_RAW_SIZE) {
            int n = recv(sock, rx_buf + offset, remaining, 0);
            if (n <= 0) {
                ESP_LOGW(TAG, "recv error at offset %zu (errno %d)", offset, errno);
                break;
            }
            offset += n;
            remaining -= n;
        }

        close(sock);

        if (offset >= NOTIFY_RAW_SIZE) {
            ESP_LOGI(TAG, "Full frame received from %s (%zu bytes, LE RGB565). Updating display...", client_ip, offset);
            lcd_display_show_notification(rx_buf, duration_ms);
        } else {
            ESP_LOGW(TAG, "Incomplete frame from %s (%zu/%d bytes), discarded", client_ip, offset, NOTIFY_RAW_SIZE);
        }
    }

    free(rx_buf);
    vTaskDelete(NULL);
}

esp_err_t notify_server_init(void) {
    // mDNS will be fully set up once IP is available (called from app_event_cb).
    // Attempt early init here in case IP is already assigned.
    notify_server_init_mdns();

    BaseType_t ret = xTaskCreate(notify_server_task, "notify_srv", 4096, NULL, 4, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create notify_server task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Notification Server initialized on port %d", NOTIFY_TCP_PORT);
    return ESP_OK;
}
