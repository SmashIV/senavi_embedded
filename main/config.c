#include "config.h"

#include <errno.h>
#include <stdlib.h>

#include "nvs.h"
#include "nvs_flash.h"

struct config {
	nvs_handle_t handle;
};

static const char *config_key_name(enum config_key key)
{
	switch (key) {
	case CONFIG_DEVICE_ID:
		return "device_id";
	case CONFIG_MQTT_HOST:
		return "mqtt_host";
	case CONFIG_MQTT_PORT:
		return "mqtt_port";
	case CONFIG_MQTT_USER:
		return "mqtt_user";
	case CONFIG_MQTT_PASS:
		return "mqtt_pass";
	case CONFIG_MQTT_TOPIC:
		return "mqtt_topic";
	case CONFIG_TEMP_INTERVAL_MS:
		return "temp_interval_ms";
	case CONFIG_GAS_INTERVAL_MS:
		return "gas_interval_ms";
	case CONFIG_GPS_INTERVAL_MS:
		return "gps_interval_ms";
	case CONFIG_IMU_INTERVAL_MS:
		return "imu_interval_ms";
	default:
		return NULL;
	}
}

int config_init(struct config **configp)
{
	struct config *config;
	esp_err_t err;

	if (!configp)
		return -EINVAL;
	*configp = NULL;

	err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
	    err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		err = nvs_flash_erase();
		if (err != ESP_OK)
			return -(int) err;
		err = nvs_flash_init();
	}
	if (err != ESP_OK)
		return -(int) err;

	config = calloc(1, sizeof(*config));
	if (!config)
		return -ENOMEM;

	err = nvs_open("senavi", NVS_READWRITE, &config->handle);
	if (err != ESP_OK) {
		free(config);
		return -(int) err;
	}

	*configp = config;
	return 0;
}

void config_deinit(struct config *configp)
{
	if (!configp)
		return;
	nvs_close(configp->handle);
	free(configp);
}

int config_get_string(const struct config *configp, enum config_key key,
		      char *value, size_t value_size)
{
	const char *name;
	size_t required_size;
	esp_err_t err;

	if (!configp || !value || value_size == 0)
		return -EINVAL;
	name = config_key_name(key);
	if (!name)
		return -EINVAL;

	required_size = value_size;
	err = nvs_get_str(configp->handle, name, value, &required_size);
	if (err == ESP_ERR_NVS_NOT_FOUND)
		return -ENOENT;
	if (err == ESP_ERR_NVS_INVALID_LENGTH)
		return -ENOSPC;
	if (err != ESP_OK)
		return -(int) err;
	return 0;
}

int config_get_u32(const struct config *configp, enum config_key key,
		   uint32_t *value)
{
	const char *name;
	esp_err_t err;

	if (!configp || !value)
		return -EINVAL;
	name = config_key_name(key);
	if (!name)
		return -EINVAL;

	err = nvs_get_u32(configp->handle, name, value);
	if (err == ESP_ERR_NVS_NOT_FOUND)
		return -ENOENT;
	if (err != ESP_OK)
		return -(int) err;
	return 0;
}
