
#include "libretro.h"
#include "logging.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HEX_WIDTH 16

static retro_log_printf_t my_log_cb = fallback_log;
static struct retro_log_callback logging;

struct retro_log_callback *get_log_callback(void)
{
	return &logging;
}

void set_log_printf(void)
{
	my_log_cb = logging.log;
}

void log_cb(enum retro_log_level level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buffer[1024]; // Define a suitable size for your needs
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	my_log_cb(level, "%s", buffer);
	va_end(args);
}

void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
	(void)level;
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

void hexdump_log_cb(enum retro_log_level level, const void *data, size_t size)
{
	const uint8_t *byte_data = (const uint8_t *)data;
	char line_buffer[HEX_WIDTH * 3 + 1]; // 2 chars per byte and 1 space, +1 for null terminator
	int buffer_index = 0;

	for (size_t i = 0; i < size; ++i) {
		buffer_index += snprintf(line_buffer + buffer_index,
					 sizeof(line_buffer) - buffer_index, "%02X ", byte_data[i]);

		if ((i + 1) % HEX_WIDTH == 0 || i == size - 1) {
			my_log_cb(level, "%s\n", line_buffer);
			memset(line_buffer, 0, sizeof(line_buffer));
			buffer_index = 0;
		}
	}
}
