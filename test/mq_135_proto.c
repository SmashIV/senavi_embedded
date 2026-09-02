#include <stdint.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MQ_135_PROTO_ADC_UNIT     ADC_UNIT_1
#define MQ_135_PROTO_ADC_CHANNEL  ADC_CHANNEL_6
#define MQ_135_PROTO_ATTEN        ADC_ATTEN_DB_12
#define MQ_135_PROTO_INTERVAL_MS  1000

static const char *mq_135_proto_tag = "mq_135_proto";
static adc_oneshot_unit_handle_t mq_135_proto_adc;

static int mq_135_proto_init(void)
{
	adc_oneshot_unit_init_cfg_t unit_config = {
		.unit_id = MQ_135_PROTO_ADC_UNIT,
	};
	adc_oneshot_chan_cfg_t channel_config = {
		.atten = MQ_135_PROTO_ATTEN,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
	};
	esp_err_t ret;

	ret = adc_oneshot_new_unit(&unit_config, &mq_135_proto_adc);
	if (ret != ESP_OK)
		return ret;

	ret = adc_oneshot_config_channel(mq_135_proto_adc,
					 MQ_135_PROTO_ADC_CHANNEL,
					 &channel_config);
	if (ret != ESP_OK) {
		adc_oneshot_del_unit(mq_135_proto_adc);
		return ret;
	}

	return 0;
}

void mq_135_proto_run(void)
{
	int ret;
	int raw;

	ret = mq_135_proto_init();
	if (ret != 0) {
		ESP_LOGE(mq_135_proto_tag, "ADC init failed: %d", ret);
		return;
	}

	ESP_LOGI(mq_135_proto_tag,
		 "MQ-135 gas sensor test on GPIO 34 (ADC1_CH6)");
	ESP_LOGW(mq_135_proto_tag,
		 "MQ-135 needs preheat and calibration for ppm conversion");

	while (1) {
		ret = adc_oneshot_read(mq_135_proto_adc,
				       MQ_135_PROTO_ADC_CHANNEL, &raw);
		if (ret != ESP_OK) {
			ESP_LOGE(mq_135_proto_tag, "ADC read failed: %d", ret);
		} else {
			float voltage;

			voltage = (float) raw * 3.3f / 4095.0f;
			ESP_LOGI(mq_135_proto_tag,
				 "raw=%d  voltage=%.3f V", raw, voltage);
		}
		vTaskDelay(pdMS_TO_TICKS(MQ_135_PROTO_INTERVAL_MS));
	}
}
