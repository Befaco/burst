// DebugUtils.h

#include <Arduino.h>
#include <stdarg.h>

// #define DEBUG

void serialPrintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void printDouble(double val, unsigned int precision);

#ifdef DEBUG
#define SERIAL_PRINTLN(msg, ...) \
	serialPrintf(msg "\n", ##__VA_ARGS__)
#else
#define SERIAL_PRINTLN(msg, ...)
#endif