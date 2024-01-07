#include "logging.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HEX_WIDTH 16

void log_cb(const char *level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buffer[1024]; // Define a suitable size for your needs
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	printf("%s %s", level, buffer);
	va_end(args);
}

void hexdump_log_cb(const char *level, const void *data, size_t size)
{
	const uint8_t *byte_data = (const uint8_t *)data;
	char line_buffer[HEX_WIDTH * 3 + 1]; // 2 chars per byte and 1 space, +1 for null terminator
	int buffer_index = 0;

	for (size_t i = 0; i < size; ++i) {
		buffer_index += snprintf(line_buffer + buffer_index,
					 sizeof(line_buffer) - buffer_index, "%02X ", byte_data[i]);

		if ((i + 1) % HEX_WIDTH == 0 || i == size - 1) {
			printf("%s %s\n", level, line_buffer);
			memset(line_buffer, 0, sizeof(line_buffer));
			buffer_index = 0;
		}
	}
}
