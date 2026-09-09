#include "ublox_neo_6m.h"

#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UBX_NEO_6M_BAUD       9600
#define UBX_NEO_6M_BUF_SIZE   512
#define UBX_NEO_6M_READ_TICKS pdMS_TO_TICKS(50)
#define UBX_NEO_6M_READ_MS    800

struct ubx_neo_6m {
	int uart_num;
	char *buf;
	size_t buf_len;
};

static float nmea_to_decimal(float raw)
{
	int degrees;
	float minutes;

	degrees = (int)(raw / 100.0f);
	minutes = raw - (float)degrees * 100.0f;
	return (float)degrees + minutes / 60.0f;
}

static int parse_gga(const char *sentence,
		     struct ubx_neo_6m_reading *readingp)
{
	float lat_raw;
	float lon_raw;
	char ns;
	char ew;
	int quality;
	int satellites;
	float altitude;
	int matched;

	matched = sscanf(sentence,
			 "$GPGGA,%*f,%f,%c,%f,%c,%d,%d,%*f,%f,%*c",
			 &lat_raw, &ns, &lon_raw, &ew, &quality,
			 &satellites, &altitude);
	if (matched < 7)
		return UBX_NEO_6M_ERR_PARSE;
	if (quality == 0) {
		readingp->has_fix = false;
		return UBX_NEO_6M_ERR_NOFIX;
	}

	readingp->latitude = nmea_to_decimal(lat_raw);
	readingp->longitude = nmea_to_decimal(lon_raw);
	if (ns == 'S')
		readingp->latitude = -readingp->latitude;
	if (ew == 'W')
		readingp->longitude = -readingp->longitude;
	readingp->num_satellites = (uint8_t) satellites;
	readingp->altitude_m = altitude;
	readingp->has_fix = true;

	return 0;
}

static int parse_rmc(const char *sentence,
		     struct ubx_neo_6m_reading *readingp)
{
	char status;
	float speed_knots;
	float course;
	int matched;

	matched = sscanf(sentence, "$GPRMC,%*f,%c,%*f,%*c,%*f,%*c,%f,%f,%*s",
			 &status, &speed_knots, &course);
	if (matched < 3 || status != 'A')
		return UBX_NEO_6M_ERR_PARSE;

	readingp->speed_knots = speed_knots;
	readingp->course_deg = course;

	return 0;
}

static void process_line(char *line, struct ubx_neo_6m_reading *readingp)
{
	if (strncmp(line, "$GPGGA,", 7) == 0)
		parse_gga(line, readingp);
	else if (strncmp(line, "$GPRMC,", 7) == 0)
		parse_rmc(line, readingp);
}

struct ubx_neo_6m *ubx_neo_6m_init(int uart_num, int tx_pin, int rx_pin)
{
	const uart_config_t uart_config = {
		.baud_rate = UBX_NEO_6M_BAUD,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	struct ubx_neo_6m *gpsp;
	esp_err_t ret;

	if (uart_num < UART_NUM_0 || uart_num >= UART_NUM_MAX)
		return NULL;

	gpsp = malloc(sizeof(*gpsp));
	if (!gpsp)
		return NULL;
	gpsp->buf = malloc(UBX_NEO_6M_BUF_SIZE);
	if (!gpsp->buf) {
		free(gpsp);
		return NULL;
	}
	gpsp->uart_num = uart_num;
	gpsp->buf_len = 0;

	ret = uart_driver_install(uart_num, UBX_NEO_6M_BUF_SIZE, 0, 0,
				  NULL, 0);
	if (ret != ESP_OK)
		goto free_all;
	ret = uart_param_config(uart_num, &uart_config);
	if (ret != ESP_OK) {
		uart_driver_delete(uart_num);
		goto free_all;
	}
	ret = uart_set_pin(uart_num, tx_pin, rx_pin,
			   UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	if (ret != ESP_OK) {
		uart_driver_delete(uart_num);
		goto free_all;
	}

	return gpsp;

free_all:
	free(gpsp->buf);
	free(gpsp);
	return NULL;
}

int ubx_neo_6m_read(struct ubx_neo_6m *gpsp,
		    struct ubx_neo_6m_reading *readingp)
{
	struct ubx_neo_6m_reading reading = { 0 };
	int64_t deadline_us;
	int ret;

	if (!gpsp || !readingp)
		return UBX_NEO_6M_ERR_ARG;

	deadline_us = esp_timer_get_time() +
		(int64_t) UBX_NEO_6M_READ_MS * 1000;

	while (esp_timer_get_time() < deadline_us) {
		int len;
		size_t i;
		size_t line_start = 0;

		len = uart_read_bytes(gpsp->uart_num,
				      (uint8_t *) gpsp->buf + gpsp->buf_len,
				      UBX_NEO_6M_BUF_SIZE - gpsp->buf_len - 1,
				      UBX_NEO_6M_READ_TICKS);
		if (len < 0)
			return UBX_NEO_6M_ERR_UART;
		if (len == 0)
			continue;

		gpsp->buf_len += (size_t) len;
		gpsp->buf[gpsp->buf_len] = '\0';

		for (i = 0; i < gpsp->buf_len; i++) {
			if (gpsp->buf[i] != '\n')
				continue;
			gpsp->buf[i] = '\0';
			if (i > line_start && gpsp->buf[line_start] == '$') {
				process_line(&gpsp->buf[line_start], &reading);
				if (reading.has_fix && readingp)
					*readingp = reading;
			}
			line_start = i + 1;
		}

		if (line_start > 0 && line_start < gpsp->buf_len)
			memmove(gpsp->buf, &gpsp->buf[line_start],
				gpsp->buf_len - line_start);
		gpsp->buf_len -= line_start;
		gpsp->buf[gpsp->buf_len] = '\0';

		if (reading.has_fix) {
			*readingp = reading;
			return 0;
		}
	}

	return UBX_NEO_6M_ERR_NOFIX;
}

void ubx_neo_6m_deinit(struct ubx_neo_6m *gpsp)
{
	if (!gpsp)
		return;
	uart_driver_delete(gpsp->uart_num);
	free(gpsp->buf);
	free(gpsp);
}
