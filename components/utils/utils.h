#ifndef UTILS_H
#define UTILS_H

#include <esp_log.h>

#define __YEAR__                                                               \
  ((((__DATE__[7] - '0') * 10 + (__DATE__[8] - '0')) * 10 +                    \
    (__DATE__[9] - '0')) *                                                     \
       10 +                                                                    \
   (__DATE__[10] - '0'))

#define __MONTH__                                                              \
  (__DATE__[2] == 'n'   ? (__DATE__[1] == 'a' ? 1 : 6)                         \
   : __DATE__[2] == 'b' ? 2                                                    \
   : __DATE__[2] == 'r' ? (__DATE__[0] == 'M' ? 3 : 4)                         \
   : __DATE__[2] == 'y' ? 5                                                    \
   : __DATE__[2] == 'l' ? 7                                                    \
   : __DATE__[2] == 'g' ? 8                                                    \
   : __DATE__[2] == 'p' ? 9                                                    \
   : __DATE__[2] == 't' ? 10                                                   \
   : __DATE__[2] == 'v' ? 11                                                   \
                        : 12)

#define __DAY__                                                                \
  (__DATE__[4] == ' ' ? (__DATE__[5] - '0')                                    \
                      : ((__DATE__[4] - '0') * 10 + (__DATE__[5] - '0')))

void hexdump(const uint8_t *buf, uint32_t len);

#endif // UTILS_H
