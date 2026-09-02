#include "time.h"

#include <errno.h>
#include <sys/time.h>

int time_now_epoch_ms(int64_t *epoch_ms)
{
	struct timeval current_time;

	if (!epoch_ms)
		return -EINVAL;
	if (gettimeofday(&current_time, NULL) != 0)
		return -errno;
	*epoch_ms = (int64_t) current_time.tv_sec * 1000;
	*epoch_ms += current_time.tv_usec / 1000;
	return 0;
}
