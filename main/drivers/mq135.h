#ifndef MQ135_H
#define MQ135_H

#include <stdint.h>

#define MQ135_ERR_ADC   (-1)
#define MQ135_ERR_CALI  (-2)
#define MQ135_ERR_ARG   (-3)

struct mq135;

struct mq135_reading {
	uint32_t voltage_mv;
	uint16_t raw;
};

struct mq135 *mq135_init(int gpio_num);
int mq135_read(struct mq135 *mq135p, struct mq135_reading *readingp);
void mq135_deinit(struct mq135 *mq135p);

#endif /* MQ135_H */
