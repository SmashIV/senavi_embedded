#ifndef SENAVI_LOG_H
#define SENAVI_LOG_H

#include <stdarg.h>

#include "esp_log.h"

enum log_level {
	LOG_LEVEL_ERROR = ESP_LOG_ERROR,
	LOG_LEVEL_WARN = ESP_LOG_WARN,
	LOG_LEVEL_INFO = ESP_LOG_INFO,
	LOG_LEVEL_DEBUG = ESP_LOG_DEBUG,
};

void log_set_level(enum log_level level);
void log_write(enum log_level level, const char *format, ...);
void log_writev(enum log_level level, const char *format, va_list args);

#endif
