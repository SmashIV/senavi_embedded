#include "sensors_collector.h"

#include <stdio.h>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "drivers/dht22.h"
#include "drivers/mq135.h"
#include "drivers/ublox_neo_6m.h"

#define DHT22_GPIO_NUM     26
#define MQ135_GPIO_NUM     34
#define GPS_UART_NUM       UART_NUM_2
#define GPS_TX_PIN         17
#define GPS_RX_PIN         16

#define DHT22_INTERVAL_MS  2000
#define MQ135_INTERVAL_MS  1000
#define GPS_INTERVAL_MS    1000

#define TASK_STACK_SIZE    4096
#define TASK_PRIORITY      5

#define LINE_BUF_SIZE      128

static void serialize_dht(const struct dht22_reading *reading,
			  char *line, size_t line_size)
{
	snprintf(line, line_size,
		 "{\"type\":\"dht\",\"hum\":%.1f,\"temp\":%.1f}\n",
		 reading->humidity_tenths / 10.0f,
		 reading->temperature_tenths / 10.0f);
}

static void serialize_gas(const struct mq135_reading *reading,
			  char *line, size_t line_size)
{
	snprintf(line, line_size,
		 "{\"type\":\"gas\",\"raw\":%u,\"volt\":%.3f}\n",
		 reading->raw, reading->voltage_mv / 1000.0f);
}

static void serialize_gps(const struct ubx_neo_6m_reading *reading,
			  char *line, size_t line_size)
{
	snprintf(line, line_size,
		 "{\"type\":\"gps\",\"lat\":%.5f,\"lon\":%.5f,"
		 "\"alt\":%.1f,\"sats\":%u}\n",
		 reading->latitude, reading->longitude,
		 reading->altitude_m, reading->num_satellites);
}

static void serialize_error(const char *type, int code,
			    char *line, size_t line_size)
{
	snprintf(line, line_size,
		 "{\"type\":\"%s\",\"err\":%d}\n", type, code);
}

static void dht_task(void *arg)
{
	struct dht22 *dht = arg;
	struct dht22_reading reading;
	char line[LINE_BUF_SIZE];
	int ret;

	while (1) {
		ret = dht22_read(dht, &reading);
		if (ret == 0)
			serialize_dht(&reading, line, sizeof(line));
		else
			serialize_error("dht", ret, line, sizeof(line));
		printf("%s", line);
		vTaskDelay(pdMS_TO_TICKS(DHT22_INTERVAL_MS));
	}
}

static void gas_task(void *arg)
{
	struct mq135 *gas = arg;
	struct mq135_reading reading;
	char line[LINE_BUF_SIZE];
	int ret;

	while (1) {
		ret = mq135_read(gas, &reading);
		if (ret == 0)
			serialize_gas(&reading, line, sizeof(line));
		else
			serialize_error("gas", ret, line, sizeof(line));
		printf("%s", line);
		vTaskDelay(pdMS_TO_TICKS(MQ135_INTERVAL_MS));
	}
}

static void gps_task(void *arg)
{
	struct ubx_neo_6m *gps = arg;
	struct ubx_neo_6m_reading reading;
	char line[LINE_BUF_SIZE];
	int ret;

	while (1) {
		ret = ubx_neo_6m_read(gps, &reading);
		if (ret == 0)
			serialize_gps(&reading, line, sizeof(line));
		else
			serialize_error("gps", ret, line, sizeof(line));
		printf("%s", line);
		vTaskDelay(pdMS_TO_TICKS(GPS_INTERVAL_MS));
	}
}

void sensors_collector_start(void)
{
	struct dht22 *dht;
	struct mq135 *gas;
	struct ubx_neo_6m *gps;

	dht = dht22_init(DHT22_GPIO_NUM);
	gas = mq135_init(MQ135_GPIO_NUM);
	gps = ubx_neo_6m_init(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN);

	if (!dht || !gas || !gps) {
		if (dht)
			dht22_deinit(dht);
		if (gas)
			mq135_deinit(gas);
		if (gps)
			ubx_neo_6m_deinit(gps);
		return;
	}

	xTaskCreate(dht_task, "dht_collect", TASK_STACK_SIZE,
		    dht, TASK_PRIORITY, NULL);
	xTaskCreate(gas_task, "gas_collect", TASK_STACK_SIZE,
		    gas, TASK_PRIORITY, NULL);
	xTaskCreate(gps_task, "gps_collect", TASK_STACK_SIZE,
		    gps, TASK_PRIORITY, NULL);
}