#include "mq135.h"

#include <stdlib.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#define MQ135_ATTEN          ADC_ATTEN_DB_12
#define MQ135_DEFAULT_VREF   1100
#define MQ135_OVERSAMPLE     64

struct mq135 {
	adc_oneshot_unit_handle_t adc_unit;
	adc_cali_handle_t adc_cali;
	adc_channel_t channel;
};

static const struct {
	int gpio_num;
	adc_channel_t channel;
} mq135_gpio_channel[] = {
	{ GPIO_NUM_32, ADC_CHANNEL_4 },
	{ GPIO_NUM_33, ADC_CHANNEL_5 },
	{ GPIO_NUM_34, ADC_CHANNEL_6 },
	{ GPIO_NUM_35, ADC_CHANNEL_7 },
	{ GPIO_NUM_36, ADC_CHANNEL_0 },
	{ GPIO_NUM_37, ADC_CHANNEL_1 },
	{ GPIO_NUM_38, ADC_CHANNEL_2 },
	{ GPIO_NUM_39, ADC_CHANNEL_3 },
};

static int mq135_gpio_to_channel(int gpio_num, adc_channel_t *channelp)
{
	size_t i;

	for (i = 0; i < sizeof(mq135_gpio_channel) /
		     sizeof(mq135_gpio_channel[0]); i++) {
		if (mq135_gpio_channel[i].gpio_num == gpio_num) {
			*channelp = mq135_gpio_channel[i].channel;
			return 0;
		}
	}
	return MQ135_ERR_ARG;
}

struct mq135 *mq135_init(int gpio_num)
{
	struct mq135 *mq135p;
	adc_oneshot_unit_init_cfg_t unit_config;
	adc_oneshot_chan_cfg_t channel_config;
	adc_cali_line_fitting_config_t cali_config;
	adc_channel_t channel;
	esp_err_t ret;

	if (mq135_gpio_to_channel(gpio_num, &channel) != 0)
		return NULL;

	mq135p = malloc(sizeof(*mq135p));
	if (!mq135p)
		return NULL;
	mq135p->channel = channel;

	unit_config = (adc_oneshot_unit_init_cfg_t) {
		.unit_id = ADC_UNIT_1,
	};
	channel_config = (adc_oneshot_chan_cfg_t) {
		.atten = MQ135_ATTEN,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
	};
	cali_config = (adc_cali_line_fitting_config_t) {
		.unit_id = ADC_UNIT_1,
		.atten = MQ135_ATTEN,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.default_vref = MQ135_DEFAULT_VREF,
	};

	ret = adc_oneshot_new_unit(&unit_config, &mq135p->adc_unit);
	if (ret != ESP_OK)
		goto free_ctx;
	ret = adc_oneshot_config_channel(mq135p->adc_unit, channel,
					 &channel_config);
	if (ret != ESP_OK)
		goto del_unit;
	ret = adc_cali_create_scheme_line_fitting(&cali_config,
						  &mq135p->adc_cali);
	if (ret != ESP_OK)
		goto del_unit;

	return mq135p;

del_unit:
	adc_oneshot_del_unit(mq135p->adc_unit);
free_ctx:
	free(mq135p);
	return NULL;
}

int mq135_read(struct mq135 *mq135p, struct mq135_reading *readingp)
{
	uint32_t sum;
	int voltage_mv;
	int raw;
	int avg_raw;
	int i;
	esp_err_t ret;

	if (!mq135p || !readingp)
		return MQ135_ERR_ARG;

	sum = 0;
	for (i = 0; i < MQ135_OVERSAMPLE; i++) {
		ret = adc_oneshot_read(mq135p->adc_unit, mq135p->channel, &raw);
		if (ret != ESP_OK)
			return MQ135_ERR_ADC;
		sum += (uint32_t) raw;
	}

	avg_raw = (int) (sum / MQ135_OVERSAMPLE);
	ret = adc_cali_raw_to_voltage(mq135p->adc_cali, avg_raw, &voltage_mv);
	if (ret != ESP_OK)
		return MQ135_ERR_CALI;

	readingp->raw = (uint16_t) avg_raw;
	readingp->voltage_mv = (uint32_t) voltage_mv;

	return 0;
}

void mq135_deinit(struct mq135 *mq135p)
{
	if (!mq135p)
		return;
	adc_cali_delete_scheme_line_fitting(mq135p->adc_cali);
	adc_oneshot_del_unit(mq135p->adc_unit);
	free(mq135p);
}
