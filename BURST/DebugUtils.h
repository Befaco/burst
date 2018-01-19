// DebugUtils.h

#include <Arduino.h>
#include <stdarg.h>

// #define DEBUG
// #define EOC_OUT_IS_TEMPO // enable to use EOC out as tempo output
#define TEMPO_LED       0 // ALIAS
#define TEMPO_STATE     9 // ALIAS

void serialPrintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void printDouble(double val, unsigned int precision);

#ifdef DEBUG
#define SERIAL_PRINTLN(msg, ...) \
	serialPrintf(msg "\n", ##__VA_ARGS__)
#else
#define SERIAL_PRINTLN(msg, ...)
#endif