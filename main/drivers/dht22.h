#ifndef DHT22_H
#define DHT22_H

#include <stdint.h>

#define DHT22_ERR_TIMEOUT      (-1)
#define DHT22_ERR_CHECKSUM     (-2)
#define DHT22_ERR_INVALID_DATA (-3)
#define DHT22_ERR_ARG          (-4)

struct dht22;

struct dht22_reading {
	int16_t temperature_tenths;
	uint16_t humidity_tenths;
};

struct dht22 *dht22_init(int gpio_num);
int dht22_read(struct dht22 *dht22p, struct dht22_reading *readingp);
void dht22_deinit(struct dht22 *dht22p);

#endif /* DHT22_H */
