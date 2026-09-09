#include <stdint.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MQ_135_ADC_UNIT      ADC_UNIT_1
#define MQ_135_ADC_CHANNEL   ADC_CHANNEL_6
#define MQ_135_ATTEN         ADC_ATTEN_DB_12
#define MQ_135_DEFAULT_VREF  1100
#define MQ_135_OVERSAMPLE    64
#define MQ_135_INTERVAL_MS   1000

static const char *mq_135_proto_tag = "mq_135_proto";
static adc_oneshot_unit_handle_t mq_135_adc;
static adc_cali_handle_t mq_135_cali;

static int mq_135_proto_init(void)
{
	adc_oneshot_unit_init_cfg_t unit_config = {
		.unit_id = MQ_135_ADC_UNIT,
	};
	adc_oneshot_chan_cfg_t channel_config = {
		.atten = MQ_135_ATTEN,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
	};
	adc_cali_line_fitting_config_t cali_config = {
		.unit_id = MQ_135_ADC_UNIT,
		.atten = MQ_135_ATTEN,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.default_vref = MQ_135_DEFAULT_VREF,
	};
	esp_err_t ret;

	ret = adc_oneshot_new_unit(&unit_config, &mq_135_adc);
	if (ret != ESP_OK)
		return ret;

	ret = adc_oneshot_config_channel(mq_135_adc,
					 MQ_135_ADC_CHANNEL, &channel_config);
	if (ret != ESP_OK) {
		adc_oneshot_del_unit(mq_135_adc);
		return ret;
	}

	ret = adc_cali_create_scheme_line_fitting(&cali_config, &mq_135_cali);
	if (ret != ESP_OK) {
		adc_oneshot_del_unit(mq_135_adc);
		return ret;
	}

	return 0;
}

void mq_135_proto_run(void)
{
	int ret;
	int raw;
	int avg_raw;
	int voltage_mv;
	uint32_t adc_sum;
	int i;

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
		adc_sum = 0;
		for (i = 0; i < MQ_135_OVERSAMPLE; i++) {
			ret = adc_oneshot_read(mq_135_adc,
					       MQ_135_ADC_CHANNEL, &raw);
			if (ret != ESP_OK)
				break;
			adc_sum += (uint32_t) raw;
		}
		if (ret != ESP_OK) {
			ESP_LOGE(mq_135_proto_tag, "ADC read failed: %d", ret);
		} else {
			float voltage;

			avg_raw = (int) (adc_sum / MQ_135_OVERSAMPLE);
			ret = adc_cali_raw_to_voltage(mq_135_cali,
						      avg_raw, &voltage_mv);
			if (ret != ESP_OK) {
				ESP_LOGE(mq_135_proto_tag,
					 "ADC calibration failed: %d", ret);
			} else {
				voltage = (float) voltage_mv / 1000.0f;
				ESP_LOGI(mq_135_proto_tag,
					 "raw=%d  voltage=%.3f V",
					 avg_raw, voltage);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(MQ_135_INTERVAL_MS));
	}
}
