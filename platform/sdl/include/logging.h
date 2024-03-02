
#ifndef LOGGING_H_
#define LOGGING_H_

#include <stddef.h>

void log_cb(const char *level, const char *fmt, ...);
void hexdump_log_cb(const char *level, const void *data, size_t size);

#define LOG_DBG_CB(fmt, ...)                                                                       \
	do {                                                                                       \
		log_cb("DEBUG", fmt "\n", ##__VA_ARGS__);                                          \
	} while (0)

#define LOG_INF_CB(fmt, ...)                                                                       \
	do {                                                                                       \
		log_cb("INFO", fmt "\n", ##__VA_ARGS__);                                           \
	} while (0)

#define LOG_WRN_CB(fmt, ...)                                                                       \
	do {                                                                                       \
		log_cb("WARN", fmt "\n", ##__VA_ARGS__);                                           \
	} while (0)

#define LOG_ERR_CB(fmt, ...)                                                                       \
	do {                                                                                       \
		log_cb("ERROR", fmt "\n", ##__VA_ARGS__);                                          \
	} while (0)

#define LOG_DBG(fmt, ...) LOG_DBG_CB(fmt, ##__VA_ARGS__)
#define LOG_INF(fmt, ...) LOG_INF_CB(fmt, ##__VA_ARGS__)
#define LOG_WRN(fmt, ...) LOG_WRN_CB(fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) LOG_ERR_CB(fmt, ##__VA_ARGS__)

#define LOG_HEXDUMP_DBG_CB(data, size) hexdump_log_cb("DEBUG", data, size)
#define LOG_HEXDUMP_INF_CB(data, size) hexdump_log_cb("INFO", data, size)
#define LOG_HEXDUMP_WRN_CB(data, size) hexdump_log_cb("WARN", data, size)
#define LOG_HEXDUMP_ERR_CB(data, size) hexdump_log_cb("ERROR", data, size)

#define LOG_HEXDUMP_DBG(data, size) LOG_HEXDUMP_DBG_CB(data, size)
#define LOG_HEXDUMP_INF(data, size) LOG_HEXDUMP_INF_CB(data, size)
#define LOG_HEXDUMP_WRN(data, size) LOG_HEXDUMP_WRN_CB(data, size)
#define LOG_HEXDUMP_ERR(data, size) LOG_HEXDUMP_ERR_CB(data, size)

#endif /* LOGGING_H_ */
