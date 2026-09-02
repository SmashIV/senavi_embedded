#include "log.h"

static const char *log_tag = "senavi";

void log_set_level(enum log_level level)
{
	esp_log_level_set(log_tag, (esp_log_level_t) level);
}

void log_writev(enum log_level level, const char *format, va_list args)
{
	esp_log_writev((esp_log_level_t) level, log_tag, format, args);
}

void log_write(enum log_level level, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	log_writev(level, format, args);
	va_end(args);
}
