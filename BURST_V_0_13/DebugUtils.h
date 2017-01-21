// DebugUtils.h

void serial_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define SERIAL_PRINTLN(msg, ...) \
	serial_printf(msg "\n", ##__VA_ARGS__)
