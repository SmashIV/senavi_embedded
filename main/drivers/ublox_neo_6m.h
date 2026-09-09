#ifndef UBX_NEO_6M_H
#define UBX_NEO_6M_H

#include <stdint.h>
#include <stdbool.h>

#define UBX_NEO_6M_ERR_UART   (-1)
#define UBX_NEO_6M_ERR_PARSE  (-2)
#define UBX_NEO_6M_ERR_ARG    (-3)
#define UBX_NEO_6M_ERR_NOFIX  (-4)

struct ubx_neo_6m;

struct ubx_neo_6m_reading {
	bool has_fix;
	uint8_t num_satellites;
	float latitude;
	float longitude;
	float altitude_m;
	float speed_knots;
	float course_deg;
};

struct ubx_neo_6m *ubx_neo_6m_init(int uart_num, int tx_pin, int rx_pin);
int ubx_neo_6m_read(struct ubx_neo_6m *gpsp,
		    struct ubx_neo_6m_reading *readingp);
void ubx_neo_6m_deinit(struct ubx_neo_6m *gpsp);

#endif /* UBX_NEO_6M_H */
