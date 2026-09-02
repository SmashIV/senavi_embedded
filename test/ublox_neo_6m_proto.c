#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UBX_NEO_6M_UART_NUM    UART_NUM_2
#define UBX_NEO_6M_TX_PIN      GPIO_NUM_17
#define UBX_NEO_6M_RX_PIN      GPIO_NUM_16
#define UBX_NEO_6M_BAUD_RATE   9600
#define UBX_NEO_6M_BUF_SIZE    512
#define UBX_NEO_6M_TIMEOUT_MS  100
#define UBX_NEO_6M_POLL_MS     1000

static const char *ubx_neo_6m_tag = "ublox_neo_6m_proto";

static float nmea_to_decimal(float raw)
{
	int degrees;
	float minutes;

	degrees = (int)(raw / 100.0f);
	minutes = raw - (float)degrees * 100.0f;
	return (float)degrees + minutes / 60.0f;
}

static void ubx_neo_6m_parse_gga(const char *sentence)
{
	float lat_raw;
	float lon_raw;
	char ns;
	char ew;
	int fix_quality;
	int satellites;
	float altitude;
	float lat;
	float lon;

	if (sscanf(sentence,
		   "$GPGGA,%*f,%f,%c,%f,%c,%d,%d,%*f,%f,%*c",
		   &lat_raw, &ns, &lon_raw, &ew, &fix_quality,
		   &satellites, &altitude) < 7)
		return;

	lat = nmea_to_decimal(lat_raw);
	lon = nmea_to_decimal(lon_raw);
	if (ns == 'S')
		lat = -lat;
	if (ew == 'W')
		lon = -lon;

	switch (fix_quality) {
	case 0:
		ESP_LOGI(ubx_neo_6m_tag,
			 "no fix  sats=%d", satellites);
		return;
	case 1:
		ESP_LOGI(ubx_neo_6m_tag,
			 "fix OK  lat=%.5f lon=%.5f alt=%.1fm  sats=%d",
			 lat, lon, altitude, satellites);
		return;
	case 2:
		ESP_LOGI(ubx_neo_6m_tag,
			 "fix DGPS  lat=%.5f lon=%.5f alt=%.1fm  sats=%d",
			 lat, lon, altitude, satellites);
		return;
	default:
		break;
	}
}

static void ubx_neo_6m_parse_rmc(const char *sentence)
{
	char status;
	float speed_knots;
	float course;

	if (sscanf(sentence,
		   "$GPRMC,%*f,%c,%*f,%*c,%*f,%*c,%f,%f,%*s",
		   &status, &speed_knots, &course) < 3)
		return;
	if (status != 'A')
		return;
	ESP_LOGI(ubx_neo_6m_tag,
		 "nav  speed=%.1f kn  course=%.1f deg",
		 speed_knots, course);
}

static void ubx_neo_6m_proto_process_line(char *line)
{
	if (strncmp(line, "$GPGGA,", 7) == 0) {
		ubx_neo_6m_parse_gga(line);
	} else if (strncmp(line, "$GPRMC,", 7) == 0) {
		ubx_neo_6m_parse_rmc(line);
	}
}

static int ubx_neo_6m_proto_init(void)
{
	const uart_config_t uart_config = {
		.baud_rate = UBX_NEO_6M_BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	int ret;

	ret = uart_driver_install(UBX_NEO_6M_UART_NUM,
				  UBX_NEO_6M_BUF_SIZE, 0, 0, NULL, 0);
	if (ret != ESP_OK)
		return ret;
	ret = uart_param_config(UBX_NEO_6M_UART_NUM, &uart_config);
	if (ret != ESP_OK) {
		uart_driver_delete(UBX_NEO_6M_UART_NUM);
		return ret;
	}
	return uart_set_pin(UBX_NEO_6M_UART_NUM,
			    UBX_NEO_6M_TX_PIN, UBX_NEO_6M_RX_PIN,
			    UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void ublox_neo_6m_proto_run(void)
{
	char buf[UBX_NEO_6M_BUF_SIZE];
	size_t buf_len = 0;
	int ret;

	ret = ubx_neo_6m_proto_init();
	if (ret != 0) {
		ESP_LOGE(ubx_neo_6m_tag, "UART init failed: %d", ret);
		return;
	}

	ESP_LOGI(ubx_neo_6m_tag,
		 "UBlox NEO-6M GPS test on UART%d (TX=%d RX=%d baud=%d)",
		 UBX_NEO_6M_UART_NUM, UBX_NEO_6M_TX_PIN,
		 UBX_NEO_6M_RX_PIN, UBX_NEO_6M_BAUD_RATE);
	ESP_LOGI(ubx_neo_6m_tag,
		 "UART2 free, no conflict with console");

	while (1) {
		int len;
		size_t line_start = 0;
		size_t i;

		len = uart_read_bytes(UBX_NEO_6M_UART_NUM,
				      (uint8_t *) buf + buf_len,
				      sizeof(buf) - buf_len - 1,
				      pdMS_TO_TICKS(UBX_NEO_6M_TIMEOUT_MS));
		if (len < 0) {
			ESP_LOGE(ubx_neo_6m_tag, "UART read error: %d", len);
			break;
		}
		if (len == 0)
			continue;

		buf_len += (size_t) len;
		buf[buf_len] = '\0';

		for (i = 0; i < buf_len; i++) {
			if (buf[i] != '\n')
				continue;
			buf[i] = '\0';
			if (i > line_start && buf[line_start] == '$')
				ubx_neo_6m_proto_process_line(&buf[line_start]);
			line_start = i + 1;
		}

		if (line_start > 0 && line_start < buf_len)
			memmove(buf, &buf[line_start], buf_len - line_start);
		buf_len -= line_start;
		buf[buf_len] = '\0';

		vTaskDelay(pdMS_TO_TICKS(UBX_NEO_6M_POLL_MS));
	}
}