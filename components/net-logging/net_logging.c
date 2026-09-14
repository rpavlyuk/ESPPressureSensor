#include <string.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
#include "freertos/ringbuf.h"
#else
#include "freertos/message_buffer.h"
#endif

#include "esp_system.h"
#include "esp_log.h"

#include "net_logging.h"

bool writeToStdout;

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
RingbufHandle_t xRingBufferUDP = NULL;
RingbufHandle_t xRingBufferTCP = NULL;
RingbufHandle_t xRingBufferMQTT = NULL;
RingbufHandle_t xRingBufferHTTP = NULL;
RingbufHandle_t xRingBufferSSE = NULL;
//RingbufHandle_t xRingBufferTrans;
#else
MessageBufferHandle_t xMessageBufferUDP = NULL;
MessageBufferHandle_t xMessageBufferTCP = NULL;
MessageBufferHandle_t xMessageBufferMQTT = NULL;
MessageBufferHandle_t xMessageBufferHTTP = NULL;
MessageBufferHandle_t xMessageBufferSSE = NULL;
//MessageBufferHandle_t xMessageBufferTrans;
#endif

// Optional: drop counters (volatile because touched from ISR potentially)
static volatile uint32_t s_drop_udp  = 0;
static volatile uint32_t s_drop_tcp  = 0;
static volatile uint32_t s_drop_mqtt = 0;
static volatile uint32_t s_drop_http = 0;
static volatile uint32_t s_drop_sse  = 0;

static inline bool in_isr(void) {
#if defined(xPortInIsrContext)
    return xPortInIsrContext();
#else
    // Fallback: most ESP-IDF builds have xPortInIsrContext; if not, assume task context
    return false;
#endif
}

#if !CONFIG_NET_LOGGING_USE_RINGBUFFER
static inline void send_to_mb(MessageBufferHandle_t mb, const char *data, size_t len,
                              volatile uint32_t *drop_counter)
{
    if (mb == NULL || data == NULL || len == 0) return;

    if (in_isr()) {
        BaseType_t hpw = pdFALSE;
        size_t s = xMessageBufferSendFromISR(mb, data, len, &hpw);
        if (s != len) {
            if (drop_counter) (*drop_counter)++;
        } else if (hpw) {
            portYIELD_FROM_ISR();
        }
    } else {
        size_t s = xMessageBufferSend(mb, data, len, 0 /* no block */);
        if (s != len) {
            if (drop_counter) (*drop_counter)++;
        }
    }
}
#else
static inline void send_to_rb(RingbufHandle_t rb, const char *data, size_t len,
                              volatile uint32_t *drop_counter)
{
    if (rb == NULL || data == NULL || len == 0) return;

    if (in_isr()) {
        BaseType_t hpw = pdFALSE;
        BaseType_t ok = xRingbufferSendFromISR(rb, data, len, &hpw);
        if (ok != pdTRUE) {
            if (drop_counter) (*drop_counter)++;
        } else if (hpw) {
            portYIELD_FROM_ISR();
        }
    } else {
        // Non-ISR variant exists: xRingbufferSend(). Use 0 ticks to avoid blocking.
        BaseType_t ok = xRingbufferSend(rb, data, len, 0 /* no block */);
        if (ok != pdTRUE) {
            if (drop_counter) (*drop_counter)++;
        }
    }
}
#endif

int logging_vprintf(const char *fmt, va_list l)
{
    // Format to local buffer for network output (do NOT printf() here; recursion risk)
    char buf[xItemSize];

    va_list lcopy;
    va_copy(lcopy, l);
    int n = vsnprintf(buf, sizeof(buf), fmt, lcopy);
    va_end(lcopy);

    if (n <= 0) {
        // Still optionally pass through to stdout
        if (writeToStdout) {
            return vprintf(fmt, l);
        }
        return 0;
    }

    // vsnprintf returns "would-have-written" size; actual may be truncated.
    size_t actual_len = strnlen(buf, sizeof(buf));

    // Optional: ensure newline (rsyslog readability). Only if space allows and not already present.
    // Comment out if you don't want modification of log lines.
    if (actual_len > 0 && actual_len < sizeof(buf)) {
        if (buf[actual_len - 1] != '\n') {
            if (actual_len + 1 < sizeof(buf)) {
                buf[actual_len] = '\n';
                buf[actual_len + 1] = '\0';
                actual_len += 1;
            }
        }
    }

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
    send_to_rb(xRingBufferUDP,  buf, actual_len, &s_drop_udp);
    send_to_rb(xRingBufferTCP,  buf, actual_len, &s_drop_tcp);
    send_to_rb(xRingBufferMQTT, buf, actual_len, &s_drop_mqtt);
    send_to_rb(xRingBufferHTTP, buf, actual_len, &s_drop_http);
    send_to_rb(xRingBufferSSE,  buf, actual_len, &s_drop_sse);
#else
    send_to_mb(xMessageBufferUDP,  buf, actual_len, &s_drop_udp);
    send_to_mb(xMessageBufferTCP,  buf, actual_len, &s_drop_tcp);
    send_to_mb(xMessageBufferMQTT, buf, actual_len, &s_drop_mqtt);
    send_to_mb(xMessageBufferHTTP, buf, actual_len, &s_drop_http);
    send_to_mb(xMessageBufferSSE,  buf, actual_len, &s_drop_sse);
#endif

    // Finally, optionally keep normal console output
    if (writeToStdout) {
        return vprintf(fmt, l);
    }
    return 0;
}

void udp_client(void *pvParameters);

esp_err_t udp_logging_init(const char *ipaddr, unsigned long port, int16_t enableStdout) {

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
	printf("start udp logging(xRingBuffer): ipaddr=[%s] port=%ld\n", ipaddr, port);
	// Create RineBuffer
	xRingBufferUDP = xRingbufferCreate(xBufferSizeBytes, RINGBUF_TYPE_NOSPLIT);
	configASSERT( xRingBufferUDP );
#else
	printf("start udp logging(xMessageBuffer): ipaddr=[%s] port=%ld\n", ipaddr, port);
	// Create MessageBuffer
	xMessageBufferUDP = xMessageBufferCreate(xBufferSizeBytes);
	configASSERT( xMessageBufferUDP );
#endif

	// Start UDP task
	PARAMETER_t param;
	param.port = port;
	strcpy(param.ipv4, ipaddr);
	param.taskHandle = xTaskGetCurrentTaskHandle();
	xTaskCreate(udp_client, "UDP", 1024*6, (void *)&param, 2, NULL);

	// Wait for ready to receive notify
	uint32_t value = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS(1000) );
	printf("udp ulTaskNotifyTake=%"PRIi32"\n", value);
	if (value == 0) {
		printf("stop udp logging\n");
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
		vRingbufferDelete(xRingBufferUDP);
		xRingBufferUDP = NULL;
#else
		vMessageBufferDelete(xMessageBufferUDP);
		xMessageBufferUDP = NULL;
#endif
	}

	// Set function used to output log entries.
	writeToStdout = enableStdout;
	esp_log_set_vprintf(logging_vprintf);
	return ESP_OK;
}

void tcp_client(void *pvParameters);

esp_err_t tcp_logging_init(const char *ipaddr, unsigned long port, int16_t enableStdout) {

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
	printf("start tcp logging(xRingBuffer): ipaddr=[%s] port=%ld\n", ipaddr, port);
	// Create RineBuffer
	xRingBufferTCP = xRingbufferCreate(xBufferSizeBytes, RINGBUF_TYPE_NOSPLIT);
	configASSERT( xRingBufferTCP );
#else
	printf("start tcp logging(xMessageBuffer): ipaddr=[%s] port=%ld\n", ipaddr, port);
	// Create MessageBuffer
	xMessageBufferTCP = xMessageBufferCreate(xBufferSizeBytes);
	configASSERT( xMessageBufferTCP );
#endif

	// Start TCP task
	PARAMETER_t param;
	param.port = port;
	strcpy(param.ipv4, ipaddr);
	param.taskHandle = xTaskGetCurrentTaskHandle();
	xTaskCreate(tcp_client, "TCP", 1024*6, (void *)&param, 2, NULL);

	// Wait for ready to receive notify
	uint32_t value = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS(1000) );
	printf("tcp ulTaskNotifyTake=%"PRIi32"\n", value);
	if (value == 0) {
		printf("stop tcp logging\n");
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
		vRingbufferDelete(xRingBufferTCP);
		xRingBufferTCP = NULL;
#else
		vMessageBufferDelete(xMessageBufferTCP);
		xMessageBufferTCP = NULL;
#endif
	}

	// Set function used to output log entries.
	writeToStdout = enableStdout;
	esp_log_set_vprintf(logging_vprintf);
	return ESP_OK;
}

void sse_server(void *pvParameters);

esp_err_t sse_logging_init(unsigned long port, int16_t enableStdout) {

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
	printf("start HTTP Server Sent Events logging(xRingBuffer): SSE server listening on port=%ld\n", port);
	// Create RineBuffer
	xRingBufferSSE = xRingbufferCreate(xBufferSizeBytes, RINGBUF_TYPE_NOSPLIT);
	configASSERT( xRingBufferSSE );
#else
	printf("start HTTP Server Sent Events logging(xMessageBuffer): SSE server starting on port=%ld\n", port);
	// Create MessageBuffer
	xMessageBufferSSE = xMessageBufferCreate(xBufferSizeBytes);
	configASSERT( xMessageBufferSSE );
#endif

	// Start SSE Server
	PARAMETER_t param;
	param.port = port;
	param.taskHandle = xTaskGetCurrentTaskHandle();
	xTaskCreate(sse_server, "HTTP SSE", 1024*6, (void *)&param, 2, NULL);

	// Wait for ready to receive notify
	uint32_t value = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS(1000) );
	printf("sse ulTaskNotifyTake=%"PRIi32"\n", value);
	if (value == 0) {
		printf("stop HTTP SSE logging\n");
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
		vRingbufferDelete(xRingBufferSSE);
		xRingBufferSSE = NULL;
#else
		vMessageBufferDelete(xMessageBufferSSE);
		xMessageBufferSSE = NULL;
#endif
	}

	// Set function used to output log entries.
	writeToStdout = enableStdout;
	esp_log_set_vprintf(logging_vprintf);
	return ESP_OK;
}

void mqtt_pub(void *pvParameters);

esp_err_t mqtt_logging_init(const char *url, char *topic, int16_t enableStdout) {

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
	printf("start mqtt logging(xRingBuffer): url=[%s] topic=[%s]\n", url, topic);
	// Create RineBuffer
	xRingBufferMQTT = xRingbufferCreate(xBufferSizeBytes, RINGBUF_TYPE_NOSPLIT);
	configASSERT( xRingBufferMQTT );
#else
	printf("start mqtt logging(xMessageBuffer): url=[%s] topic=[%s]\n", url, topic);
	// Create MessageBuffer
	xMessageBufferMQTT = xMessageBufferCreate(xBufferSizeBytes);
	configASSERT( xMessageBufferMQTT );
#endif

	// Start MQTT task
	PARAMETER_t param;
	strcpy(param.url, url);
	strcpy(param.topic, topic);
	param.taskHandle = xTaskGetCurrentTaskHandle();
	xTaskCreate(mqtt_pub, "MQTT", 1024*6, (void *)&param, 2, NULL);

	// Wait for ready to receive notify
	uint32_t value = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS(1000) );
	printf("mqtt ulTaskNotifyTake=%"PRIi32"\n", value);
	if (value == 0) {
		printf("stop mqtt logging\n");
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
		vRingbufferDelete(xRingBufferMQTT);
		xRingBufferMQTT = NULL;
#else
		vMessageBufferDelete(xMessageBufferMQTT);
		xMessageBufferMQTT = NULL;
#endif
	}

	// Set function used to output log entries.
	writeToStdout = enableStdout;
	esp_log_set_vprintf(logging_vprintf);
	return ESP_OK;
}

void http_client(void *pvParameters);

esp_err_t http_logging_init(const char *url, int16_t enableStdout) {

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
	printf("start http logging(xRingBuffer): url=[%s]\n", url);
	// Create RineBuffer
	xRingBufferHTTP = xRingbufferCreate(xBufferSizeBytes, RINGBUF_TYPE_NOSPLIT);
	configASSERT( xRingBufferHTTP );
#else
	printf("start http logging(xMessageBuffer): url=[%s]\n", url);
	// Create MessageBuffer
	xMessageBufferHTTP = xMessageBufferCreate(xBufferSizeBytes);
	configASSERT( xMessageBufferHTTP );
#endif

	// Start HTTP task
	PARAMETER_t param;
	strcpy(param.url, url);
	param.taskHandle = xTaskGetCurrentTaskHandle();
	xTaskCreate(http_client, "HTTP", 1024*4, (void *)&param, 2, NULL);

	// Wait for ready to receive notify
	uint32_t value = ulTaskNotifyTake( pdTRUE, pdMS_TO_TICKS(1000) );
	printf("http ulTaskNotifyTake=%"PRIi32"\n", value);
	if (value == 0) {
		printf("stop http logging\n");
#if CONFIG_NET_LOGGING_USE_RINGBUFFER
		vRingbufferDelete(xRingBufferHTTP);
		xRingBufferHTTP = NULL;
#else
		vMessageBufferDelete(xMessageBufferHTTP);
		xMessageBufferHTTP = NULL;
#endif
	}

	// Set function used to output log entries.
	writeToStdout = enableStdout;
	esp_log_set_vprintf(logging_vprintf);
	return ESP_OK;
}
