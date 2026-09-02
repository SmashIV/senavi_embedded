#ifndef SENAVI_CONFIG_H
#define SENAVI_CONFIG_H

#include <stddef.h>
#include <stdint.h>

struct config;

enum config_key {
	CONFIG_DEVICE_ID,
	CONFIG_MQTT_HOST,
	CONFIG_MQTT_PORT,
	CONFIG_MQTT_USER,
	CONFIG_MQTT_PASS,
	CONFIG_MQTT_TOPIC,
	CONFIG_TEMP_INTERVAL_MS,
	CONFIG_GAS_INTERVAL_MS,
	CONFIG_GPS_INTERVAL_MS,
	CONFIG_IMU_INTERVAL_MS,
	CONFIG_KEY_COUNT,
};

int config_init(struct config **configp);
void config_deinit(struct config *configp);
int config_get_string(const struct config *configp, enum config_key key,
		     char *value, size_t value_size);
int config_get_u32(const struct config *configp, enum config_key key,
		   uint32_t *value);

#endif
