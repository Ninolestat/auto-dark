#ifndef AUTODARK_THEME_H
#define AUTODARK_THEME_H

#include <windows.h>

typedef enum {
    MODE_SUN = 0,
    MODE_FIXED = 1
} ScheduleMode;

typedef struct {
    const wchar_t *name;
    double lat;
    double lon;
} CityPreset;

typedef struct {
    BOOL enabled;
    ScheduleMode mode;
    int lightHour;
    int lightMin;
    int darkHour;
    int darkMin;
    int cityIndex;      // Index into g_cities, or -1 for custom
    double lat;
    double lon;
    BOOL syncApps;
    BOOL syncSystem;
    BOOL changeWallpaper;
    wchar_t lightWallpaper[MAX_PATH];
    wchar_t darkWallpaper[MAX_PATH];
} AppConfig;

extern const CityPreset g_cities[];
extern const int g_cityCount;

// Reads/writes config in HKCU\Software\AutoDark
void ConfigLoad(AppConfig *cfg);
void ConfigSave(const AppConfig *cfg);

// Returns TRUE if Windows Apps currently use Light theme
BOOL IsSystemLightTheme(void);

// Applies Light (TRUE) or Dark (FALSE) theme to Windows and optionally updates desktop wallpaper
void ApplyWindowsTheme(BOOL light, const AppConfig *cfg);

// Evaluates current time against config and returns TRUE if Light should be active
BOOL ShouldBeLightNow(const AppConfig *cfg, int *outLightMin, int *outDarkMin);

// Windows Startup (HKCU\Software\Microsoft\Windows\CurrentVersion\Run)
BOOL IsStartupEnabled(void);
BOOL SetStartupEnabled(BOOL enable);

#endif // AUTODARK_THEME_H
