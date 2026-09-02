#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DHT_22_PROTO_GPIO GPIO_NUM_26
#define DHT_22_PROTO_TIMEOUT_US 200
#define DHT_22_PROTO_INTERVAL_MS 2000

static const char *dht_22_proto_tag = "dht_22_proto";
static portMUX_TYPE dht_22_proto_mux = portMUX_INITIALIZER_UNLOCKED;

static int dht_22_proto_wait_level(int level)
{
	int64_t start_us = esp_timer_get_time();

	while (gpio_get_level(DHT_22_PROTO_GPIO) != level) {
		if (esp_timer_get_time() - start_us >= DHT_22_PROTO_TIMEOUT_US)
			return -1;
	}
	return 0;
}

static int dht_22_proto_measure_high(uint32_t *duration_us)
{
	int64_t start_us = esp_timer_get_time();

	while (gpio_get_level(DHT_22_PROTO_GPIO) == 1) {
		if (esp_timer_get_time() - start_us >= DHT_22_PROTO_TIMEOUT_US)
			return -1;
	}
	*duration_us = (uint32_t)(esp_timer_get_time() - start_us);
	return 0;
}

static int dht_22_proto_read(uint8_t data[5])
{
	uint32_t high_duration_us;
	int bit;
	int byte;
	int ret = -1;

	for (byte = 0; byte < 5; byte++)
		data[byte] = 0;

	portENTER_CRITICAL(&dht_22_proto_mux);

	gpio_set_direction(DHT_22_PROTO_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(DHT_22_PROTO_GPIO, 0);
	esp_rom_delay_us(2000);
	gpio_set_level(DHT_22_PROTO_GPIO, 1);
	esp_rom_delay_us(30);
	gpio_set_direction(DHT_22_PROTO_GPIO, GPIO_MODE_INPUT);

	if (dht_22_proto_wait_level(0) != 0)
		goto end;
	if (dht_22_proto_wait_level(1) != 0)
		goto end;
	if (dht_22_proto_wait_level(0) != 0)
		goto end;

	for (bit = 0; bit < 40; bit++) {
		if (dht_22_proto_wait_level(1) != 0)
			goto end;
		if (dht_22_proto_measure_high(&high_duration_us) != 0)
			goto end;
		data[bit / 8] <<= 1;
		if (high_duration_us > 50)
			data[bit / 8] |= 1;
	}

	if ((uint8_t) (data[0] + data[1] + data[2] + data[3]) != data[4]) {
		ret = -2;
		goto end;
	}
	ret = 0;

end:
	portEXIT_CRITICAL(&dht_22_proto_mux);
	return ret;
}

static int dht_22_proto_init(void)
{
	int ret;

	ret = gpio_set_pull_mode(DHT_22_PROTO_GPIO, GPIO_PULLUP_ONLY);
	if (ret != 0)
		return ret;
	return gpio_set_direction(DHT_22_PROTO_GPIO, GPIO_MODE_INPUT);
}

void dht_22_proto_run(void)
{
	uint8_t data[5];
	uint16_t humidity_raw;
	int ret;

	ret = dht_22_proto_init();
	if (ret != 0) {
		ESP_LOGE(dht_22_proto_tag, "GPIO initialization failed: %d", ret);
		return;
	}

	ESP_LOGI(dht_22_proto_tag, "DHT22 communication test on GPIO %d",
		 DHT_22_PROTO_GPIO);
	ESP_LOGW(dht_22_proto_tag, "External pull-up resistor is recommended");

	while (1) {
		ret = dht_22_proto_read(data);
		if (ret == -1) {
			ESP_LOGE(dht_22_proto_tag, "No valid response from DHT22");
		} else if (ret == -2) {
			ESP_LOGE(dht_22_proto_tag, "DHT22 checksum failed");
		} else {
			uint16_t temperature_raw_unsigned;
			float temperature;

			humidity_raw = ((uint16_t) data[0] << 8) | data[1];
			temperature_raw_unsigned = ((uint16_t) (data[2] & 0x7F) << 8) |
				data[3];
			temperature = (float) temperature_raw_unsigned / 10.0f;
			if (data[2] & 0x80)
				temperature = -temperature;
			ESP_LOGI(dht_22_proto_tag,
				 "raw: %02x %02x %02x %02x %02x  humidity=%u.%u%% temperature=%.1f C",
				 data[0], data[1], data[2], data[3], data[4],
				 humidity_raw / 10, humidity_raw % 10, temperature);
		}
		vTaskDelay(pdMS_TO_TICKS(DHT_22_PROTO_INTERVAL_MS));
	}
}
