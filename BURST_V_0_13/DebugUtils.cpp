#include "DebugUtils.h"

#ifdef DEBUG
void serialPrintf(const char *fmt, ...)
{
  char buf[128];
  va_list args;
  va_start(args, fmt );
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
}

void printDouble(double val, unsigned int precision)
{
  // prints val with number of decimal places determine by precision
  // NOTE: precision is 1 followed by the number of zeros for the desired number of decimial places
  // example: printDouble( 3.1415, 100); // prints 3.14 (two decimal places)

   Serial.print(int(val));  //prints the int part
   Serial.print("."); // print the decimal point
   unsigned int frac;
   if(val >= 0)
       frac = (val - int(val)) * precision;
   else
       frac = (int(val) - val) * precision;
   Serial.println(frac,DEC);
}
#endif