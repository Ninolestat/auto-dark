#ifndef AUTODARK_SUN_H
#define AUTODARK_SUN_H

#include <windows.h>

// Calculates local sunrise and sunset times in minutes from midnight [0, 1439].
// Returns TRUE on success, FALSE in polar day/night edge cases (outputs fallback 360/1080).
BOOL CalcSunriseSunset(int year, int month, int day,
                       double latDeg, double lonDeg, int tzOffsetMinutes,
                       int *outSunriseMin, int *outSunsetMin);

// Returns current local timezone offset from UTC in minutes (including DST).
int GetLocalTzOffsetMinutes(void);

#endif // AUTODARK_SUN_H
