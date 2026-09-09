#include "dht22.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define DHT22_START_LOW_US      2000
#define DHT22_START_HIGH_US     30
#define DHT22_TIMEOUT_US        200
#define DHT22_BIT_THRESHOLD_US  50

static portMUX_TYPE dht22_spinlock = portMUX_INITIALIZER_UNLOCKED;

struct dht22 {
	int gpio_num;
};

static int dht22_wait_level(struct dht22 *dht22p, int level)
{
	int64_t start_us = esp_timer_get_time();

	while (gpio_get_level(dht22p->gpio_num) != level) {
		if (esp_timer_get_time() - start_us >= DHT22_TIMEOUT_US)
			return DHT22_ERR_TIMEOUT;
	}
	return 0;
}

static int dht22_measure_high(struct dht22 *dht22p, uint32_t *durationp)
{
	int64_t start_us = esp_timer_get_time();

	while (gpio_get_level(dht22p->gpio_num) == 1) {
		if (esp_timer_get_time() - start_us >= DHT22_TIMEOUT_US)
			return DHT22_ERR_TIMEOUT;
	}
	*durationp = (uint32_t)(esp_timer_get_time() - start_us);
	return 0;
}

struct dht22 *dht22_init(int gpio_num)
{
	struct dht22 *dht22p;
	gpio_config_t gpio_conf;
	esp_err_t ret;

	if (!GPIO_IS_VALID_GPIO(gpio_num))
		return NULL;

	dht22p = malloc(sizeof(*dht22p));
	if (!dht22p)
		return NULL;
	dht22p->gpio_num = gpio_num;

	gpio_conf = (gpio_config_t) {
		.pin_bit_mask = 1ULL << gpio_num,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	ret = gpio_config(&gpio_conf);
	if (ret != ESP_OK) {
		free(dht22p);
		return NULL;
	}

	return dht22p;
}

int dht22_read(struct dht22 *dht22p, struct dht22_reading *readingp)
{
	uint8_t data[5] = { 0 };
	uint32_t high_us;
	uint16_t temperature_raw;
	uint16_t magnitude;
	int bit;

	if (!dht22p || !readingp)
		return DHT22_ERR_ARG;

	portENTER_CRITICAL(&dht22_spinlock);

	gpio_set_direction(dht22p->gpio_num, GPIO_MODE_OUTPUT);
	gpio_set_level(dht22p->gpio_num, 0);
	esp_rom_delay_us(DHT22_START_LOW_US);
	gpio_set_level(dht22p->gpio_num, 1);
	esp_rom_delay_us(DHT22_START_HIGH_US);
	gpio_set_direction(dht22p->gpio_num, GPIO_MODE_INPUT);

	if (dht22_wait_level(dht22p, 0) != 0)
		goto fail;
	if (dht22_wait_level(dht22p, 1) != 0)
		goto fail;
	if (dht22_wait_level(dht22p, 0) != 0)
		goto fail;

	for (bit = 0; bit < 40; bit++) {
		if (dht22_wait_level(dht22p, 1) != 0)
			goto fail;
		if (dht22_measure_high(dht22p, &high_us) != 0)
			goto fail;
		data[bit / 8] <<= 1;
		if (high_us > DHT22_BIT_THRESHOLD_US)
			data[bit / 8] |= 1;
	}

	portEXIT_CRITICAL(&dht22_spinlock);

	if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
		return DHT22_ERR_CHECKSUM;

	readingp->humidity_tenths = ((uint16_t)data[0] << 8) | data[1];
	temperature_raw = ((uint16_t)data[2] << 8) | data[3];
	magnitude = temperature_raw & 0x7fff;
	readingp->temperature_tenths = (int16_t)magnitude;
	if (temperature_raw & 0x8000)
		readingp->temperature_tenths = readingp->temperature_tenths * -1;

	if (readingp->humidity_tenths == 0 && readingp->temperature_tenths == 0)
		return DHT22_ERR_INVALID_DATA;

	return 0;

fail:
	portEXIT_CRITICAL(&dht22_spinlock);
	return DHT22_ERR_TIMEOUT;
}

void dht22_deinit(struct dht22 *dht22p)
{
	free(dht22p);
}
