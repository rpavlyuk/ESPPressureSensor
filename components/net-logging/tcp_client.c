/*
	TCP Client

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
#include "freertos/ringbuf.h"
#else
#include "freertos/message_buffer.h"
#endif

#include "esp_log.h"
#include "lwip/sockets.h"
#include "netdb.h"

#include "net_logging.h"

static const char *TAG = "netlog_tcp";

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
extern RingbufHandle_t xRingBufferTCP;
#else
extern MessageBufferHandle_t xMessageBufferTCP;
#endif

/**
 * @brief Resolve hostname to IPv4 address
 */
static int resolve_dest_addr(const char *host, uint16_t port, struct sockaddr_in *out)
{
    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port   = htons(port);

    out->sin_addr.s_addr = inet_addr(host);
    if (out->sin_addr.s_addr != 0xffffffff) {
        return 0;
    }

    struct hostent *hp = gethostbyname(host);
    if (!hp || !hp->h_addr) {
        return -1;
    }

    struct ip4_addr *ip4_addr = (struct ip4_addr *)hp->h_addr;
    out->sin_addr.s_addr = ip4_addr->addr;
    return 0;
}

/**
 * @brief Open and connect TCP socket to destination address
 */
static int open_and_connect(const struct sockaddr_in *dest_addr)
{
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGW(TAG, "socket() failed: errno=%d", errno);
        return -1;
    }

    // Optional: avoid blocking forever on send()
    struct timeval tv = {
        .tv_sec = 2,
        .tv_usec = 0
    };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    int err = connect(sock, (struct sockaddr *)dest_addr, sizeof(*dest_addr)); // <-- FIXED SIZE
    if (err != 0) {
        ESP_LOGW(TAG, "connect() failed: errno=%d", errno);
        close(sock);
        return -1;
    }

    ESP_LOGI(TAG, "Connected to logging server %s:%u", inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
    return sock;
}

/**
 * @brief Send all data or fail
 */
static bool send_all_or_fail(int sock, const uint8_t *data, size_t len)
{
    size_t off = 0;
    while (off < len) {
        int ret = send(sock, data + off, len - off, 0);
        if (ret > 0) {
            off += (size_t)ret;
            continue;
        }

        if (ret == 0) {
            ESP_LOGW(TAG, "send() returned 0 (peer closed?)");
            return false;
        }

        // ret < 0
        ESP_LOGW(TAG, "send() failed: errno=%d", errno);
        return false;
    }
    return true;
}

/**
 * @brief TCP client task
 */
void tcp_client(void *pvParameters)
{
    PARAMETER_t param;
    memcpy(&param, pvParameters, sizeof(param));

    ESP_LOGI(TAG, "Start: %s:%u", param.ipv4, (unsigned)param.port);

    struct sockaddr_in dest_addr;
    if (resolve_dest_addr(param.ipv4, param.port, &dest_addr) != 0) {
        ESP_LOGE(TAG, "DNS/addr resolve failed for host '%s'", param.ipv4);
        vTaskDelete(NULL);
        return;
    }

    // Reconnect loop with backoff
    int sock = -1;
    int backoff_ms = 250;
    const int backoff_max_ms = 5000;

    while (sock < 0) {
        sock = open_and_connect(&dest_addr);
        if (sock >= 0) break;

        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        if (backoff_ms < backoff_max_ms) backoff_ms *= 2;
        if (backoff_ms > backoff_max_ms) backoff_ms = backoff_max_ms;
    }

    // Tell producer we are ready to receive
    xTaskNotifyGive(param.taskHandle);

    for (;;) {
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
        size_t received = 0;
        char *buffer = (char *)xRingbufferReceive(xRingBufferTCP, &received, portMAX_DELAY);
        if (!buffer || received == 0) {
            continue;
        }

        // IMPORTANT: ringbuffer item must be returned. If you want retry, you must copy.
        bool ok = send_all_or_fail(sock, (const uint8_t *)buffer, received);
        vRingbufferReturnItem(xRingBufferTCP, (void *)buffer);

#else
        char buffer[xItemSize];
        size_t received = xMessageBufferReceive(xMessageBufferTCP, buffer, sizeof(buffer), portMAX_DELAY);
        if (received == 0) {
            continue;
        }
        bool ok = send_all_or_fail(sock, (const uint8_t *)buffer, received);
#endif

        if (!ok) {
            // Connection likely broken: close and reconnect, but DO NOT crash device.
            if (sock != -1) {
                shutdown(sock, 0);
                close(sock);
                sock = -1;
            }

            // reconnect with backoff
            int bo = 250;
            while (sock < 0) {
                sock = open_and_connect(&dest_addr);
                if (sock >= 0) break;
                vTaskDelay(pdMS_TO_TICKS(bo));
                if (bo < backoff_max_ms) bo *= 2;
                if (bo > backoff_max_ms) bo = backoff_max_ms;
            }
        }
    }
}