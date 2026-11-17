#include "sun.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double DegToRad(double d) { return d * (M_PI / 180.0); }
static double RadToDeg(double r) { return r * (180.0 / M_PI); }

static int DayOfYear(int year, int month, int day) {
    static const int daysBeforeMonth[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    int doy = daysBeforeMonth[month - 1] + day;
    if (leap && month > 2) doy += 1;
    return doy;
}

static int ClampMinutes(int m) {
    while (m < 0) m += 1440;
    while (m >= 1440) m -= 1440;
    return m;
}

// NOAA simplified solar calculation (accurate to ~1 minute)
static BOOL CalcSunEvent(int doy, double latDeg, double lonDeg, int tzOffsetMinutes,
                         BOOL isSunrise, int *outMin) {
    // Fractional year (gamma) in radians at noon
    double gamma = (2.0 * M_PI / 365.0) * (doy - 1);

    // Equation of time (minutes)
    double eqTime = 229.18 * (0.000075
        + 0.001868 * cos(gamma)
        - 0.032077 * sin(gamma)
        - 0.014615 * cos(2.0 * gamma)
        - 0.040849 * sin(2.0 * gamma));

    // Solar declination (radians)
    double decl = 0.006918
        - 0.399912 * cos(gamma)
        + 0.070257 * sin(gamma)
        - 0.006758 * cos(2.0 * gamma)
        + 0.000907 * sin(2.0 * gamma)
        - 0.002697 * cos(3.0 * gamma)
        + 0.001480 * sin(3.0 * gamma);

    double latRad = DegToRad(latDeg);
    double zenithRad = DegToRad(90.833); // Official sunrise/sunset zenith

    double cosHA = (cos(zenithRad) / (cos(latRad) * cos(decl))) - (tan(latRad) * tan(decl));
    if (cosHA > 1.0 || cosHA < -1.0) {
        return FALSE; // Polar day or polar night
    }

    double haDeg = RadToDeg(acos(cosHA));
    if (!isSunrise) haDeg = -haDeg;

    // UTC time in minutes
    double utcMin = 720.0 - 4.0 * (lonDeg + haDeg) - eqTime;
    int localMin = (int)llround(utcMin) + tzOffsetMinutes;
    *outMin = ClampMinutes(localMin);
    return TRUE;
}

BOOL CalcSunriseSunset(int year, int month, int day,
                       double latDeg, double lonDeg, int tzOffsetMinutes,
                       int *outSunriseMin, int *outSunsetMin) {
    int doy = DayOfYear(year, month, day);
    int sr = 360, ss = 1080;
    BOOL ok1 = CalcSunEvent(doy, latDeg, lonDeg, tzOffsetMinutes, TRUE, &sr);
    BOOL ok2 = CalcSunEvent(doy, latDeg, lonDeg, tzOffsetMinutes, FALSE, &ss);
    if (outSunriseMin) *outSunriseMin = sr;
    if (outSunsetMin)  *outSunsetMin  = ss;
    return ok1 && ok2;
}

int GetLocalTzOffsetMinutes(void) {
    TIME_ZONE_INFORMATION tzi;
    DWORD res = GetTimeZoneInformation(&tzi);
    LONG bias = tzi.Bias;
    if (res == TIME_ZONE_ID_DAYLIGHT) {
        bias += tzi.DaylightBias;
    } else if (res == TIME_ZONE_ID_STANDARD) {
        bias += tzi.StandardBias;
    }
    // Windows Bias: UTC = local + Bias => local = UTC - Bias
    return (int)(-bias);
}
