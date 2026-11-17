#include "theme.h"
#include "sun.h"
#include <stdio.h>

const CityPreset g_cities[] = {
    { L"Roma",        41.9028,   12.4964 },
    { L"Milano",      45.4642,    9.1900 },
    { L"Torino",      45.0703,    7.6869 },
    { L"Napoli",      40.8518,   14.2681 },
    { L"Bologna",     44.4949,   11.3426 },
    { L"Firenze",     43.7696,   11.2558 },
    { L"Venezia",     45.4408,   12.3155 },
    { L"Palermo",     38.1157,   13.3615 },
    { L"Bari",        41.1171,   16.8719 },
    { L"Genova",      44.4056,    8.9463 },
    { L"Verona",      45.4384,   10.9916 },
    { L"Catania",     37.5079,   15.0830 },
    { L"Cagliari",    39.2238,    9.1217 },
    { L"Londra",      51.5074,   -0.1278 },
    { L"Parigi",      48.8566,    2.3522 },
    { L"Berlino",     52.5200,   13.4050 },
    { L"Madrid",      40.4168,   -3.7038 },
    { L"Zurigo",      47.3769,    8.5417 },
    { L"New York",    40.7128,  -74.0060 },
    { L"Tokyo",       35.6762,  139.6503 }
};

const int g_cityCount = (int)(sizeof(g_cities) / sizeof(g_cities[0]));

static const wchar_t *REG_CFG_KEY  = L"Software\\AutoDark";
static const wchar_t *REG_PERS_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
static const wchar_t *REG_RUN_KEY  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *REG_RUN_VAL  = L"AutoDark";

static DWORD RegReadDword(HKEY hKey, const wchar_t *name, DWORD defVal) {
    DWORD val = defVal, sz = sizeof(DWORD), type = 0;
    if (RegQueryValueExW(hKey, name, NULL, &type, (LPBYTE)&val, &sz) == ERROR_SUCCESS && type == REG_DWORD) {
        return val;
    }
    return defVal;
}

static void RegWriteDword(HKEY hKey, const wchar_t *name, DWORD val) {
    RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE *)&val, sizeof(DWORD));
}

static void RegReadStr(HKEY hKey, const wchar_t *name, wchar_t *out, DWORD cap, const wchar_t *defVal) {
    DWORD sz = cap * sizeof(wchar_t), type = 0;
    if (RegQueryValueExW(hKey, name, NULL, &type, (LPBYTE)out, &sz) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ) && sz >= sizeof(wchar_t)) {
        out[cap - 1] = L'\0';
        return;
    }
    wcsncpy(out, defVal, cap - 1);
    out[cap - 1] = L'\0';
}

static void RegWriteStr(HKEY hKey, const wchar_t *name, const wchar_t *val) {
    DWORD bytes = (DWORD)((wcslen(val) + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, name, 0, REG_SZ, (const BYTE *)val, bytes);
}

void ConfigLoad(AppConfig *cfg) {
    cfg->enabled    = TRUE;
    cfg->mode       = MODE_SUN;
    cfg->lightHour  = 7;
    cfg->lightMin   = 0;
    cfg->darkHour   = 19;
    cfg->darkMin    = 0;
    cfg->cityIndex  = 0; // Roma
    cfg->lat        = g_cities[0].lat;
    cfg->lon        = g_cities[0].lon;
    cfg->syncApps   = TRUE;
    cfg->syncSystem = TRUE;
    cfg->changeWallpaper = FALSE;
    wcsncpy(cfg->lightWallpaper, L"C:\\Windows\\Web\\Wallpaper\\Windows\\img0.jpg", MAX_PATH - 1);
    cfg->lightWallpaper[MAX_PATH - 1] = L'\0';
    wcsncpy(cfg->darkWallpaper,  L"C:\\Windows\\Web\\Wallpaper\\Windows\\img19.jpg", MAX_PATH - 1);
    cfg->darkWallpaper[MAX_PATH - 1] = L'\0';

    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_CFG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        cfg->enabled    = RegReadDword(hKey, L"Enabled", 1) != 0;
        cfg->mode       = (ScheduleMode)(RegReadDword(hKey, L"Mode", 0) == 1 ? MODE_FIXED : MODE_SUN);
        cfg->lightHour  = (int)RegReadDword(hKey, L"LightHour", 7);
        cfg->lightMin   = (int)RegReadDword(hKey, L"LightMin", 0);
        cfg->darkHour   = (int)RegReadDword(hKey, L"DarkHour", 19);
        cfg->darkMin    = (int)RegReadDword(hKey, L"DarkMin", 0);
        cfg->cityIndex  = (int)RegReadDword(hKey, L"CityIndex", 0);
        INT32 latMicro  = (INT32)RegReadDword(hKey, L"LatMicro", (DWORD)(INT32)(g_cities[0].lat * 10000.0));
        INT32 lonMicro  = (INT32)RegReadDword(hKey, L"LonMicro", (DWORD)(INT32)(g_cities[0].lon * 10000.0));
        cfg->lat        = (double)latMicro / 10000.0;
        cfg->lon        = (double)lonMicro / 10000.0;
        cfg->syncApps   = RegReadDword(hKey, L"SyncApps", 1) != 0;
        cfg->syncSystem = RegReadDword(hKey, L"SyncSystem", 1) != 0;
        cfg->changeWallpaper = RegReadDword(hKey, L"ChangeWallpaper", 0) != 0;
        RegReadStr(hKey, L"LightWallpaper", cfg->lightWallpaper, MAX_PATH, L"C:\\Windows\\Web\\Wallpaper\\Windows\\img0.jpg");
        RegReadStr(hKey, L"DarkWallpaper",  cfg->darkWallpaper,  MAX_PATH, L"C:\\Windows\\Web\\Wallpaper\\Windows\\img19.jpg");
        RegCloseKey(hKey);
    }

    if (cfg->lightHour < 0 || cfg->lightHour > 23) cfg->lightHour = 7;
    if (cfg->lightMin  < 0 || cfg->lightMin  > 59) cfg->lightMin  = 0;
    if (cfg->darkHour  < 0 || cfg->darkHour  > 23) cfg->darkHour  = 19;
    if (cfg->darkMin   < 0 || cfg->darkMin   > 59) cfg->darkMin   = 0;
    if (cfg->cityIndex >= g_cityCount) cfg->cityIndex = 0;
}

void ConfigSave(const AppConfig *cfg) {
    HKEY hKey = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_CFG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegWriteDword(hKey, L"Enabled",    cfg->enabled ? 1 : 0);
        RegWriteDword(hKey, L"Mode",       cfg->mode == MODE_FIXED ? 1 : 0);
        RegWriteDword(hKey, L"LightHour",  (DWORD)cfg->lightHour);
        RegWriteDword(hKey, L"LightMin",   (DWORD)cfg->lightMin);
        RegWriteDword(hKey, L"DarkHour",   (DWORD)cfg->darkHour);
        RegWriteDword(hKey, L"DarkMin",    (DWORD)cfg->darkMin);
        RegWriteDword(hKey, L"CityIndex",  (DWORD)cfg->cityIndex);
        RegWriteDword(hKey, L"LatMicro",   (DWORD)(INT32)(cfg->lat * 10000.0));
        RegWriteDword(hKey, L"LonMicro",   (DWORD)(INT32)(cfg->lon * 10000.0));
        RegWriteDword(hKey, L"SyncApps",   cfg->syncApps ? 1 : 0);
        RegWriteDword(hKey, L"SyncSystem", cfg->syncSystem ? 1 : 0);
        RegWriteDword(hKey, L"ChangeWallpaper", cfg->changeWallpaper ? 1 : 0);
        RegWriteStr(hKey, L"LightWallpaper", cfg->lightWallpaper);
        RegWriteStr(hKey, L"DarkWallpaper",  cfg->darkWallpaper);
        RegCloseKey(hKey);
    }
}

BOOL IsSystemLightTheme(void) {
    HKEY hKey = NULL;
    DWORD val = 1;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PERS_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        val = RegReadDword(hKey, L"AppsUseLightTheme", 1);
        RegCloseKey(hKey);
    }
    return val != 0;
}

void ApplyWindowsTheme(BOOL light, const AppConfig *cfg) {
    HKEY hKey = NULL;
    DWORD target = light ? 1 : 0;
    BOOL changed = FALSE;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PERS_KEY, 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (cfg->syncApps) {
            DWORD cur = RegReadDword(hKey, L"AppsUseLightTheme", target ^ 1);
            if (cur != target) {
                RegWriteDword(hKey, L"AppsUseLightTheme", target);
                changed = TRUE;
            }
        }
        if (cfg->syncSystem) {
            DWORD cur = RegReadDword(hKey, L"SystemUsesLightTheme", target ^ 1);
            if (cur != target) {
                RegWriteDword(hKey, L"SystemUsesLightTheme", target);
                changed = TRUE;
            }
        }
        RegCloseKey(hKey);
    }

    if (changed) {
        SendNotifyMessageW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet");
    }

    if (cfg->changeWallpaper) {
        const wchar_t *wp = light ? cfg->lightWallpaper : cfg->darkWallpaper;
        if (wp && wp[0] != L'\0' && GetFileAttributesW(wp) != INVALID_FILE_ATTRIBUTES) {
            SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)wp, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        }
    }
}

BOOL ShouldBeLightNow(const AppConfig *cfg, int *outLightMin, int *outDarkMin) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    int nowMin = st.wHour * 60 + st.wMinute;

    int lightM = cfg->lightHour * 60 + cfg->lightMin;
    int darkM  = cfg->darkHour  * 60 + cfg->darkMin;

    if (cfg->mode == MODE_SUN) {
        int tz = GetLocalTzOffsetMinutes();
        CalcSunriseSunset(st.wYear, st.wMonth, st.wDay, cfg->lat, cfg->lon, tz, &lightM, &darkM);
    }

    if (outLightMin) *outLightMin = lightM;
    if (outDarkMin)  *outDarkMin  = darkM;

    if (lightM < darkM) {
        return (nowMin >= lightM && nowMin < darkM);
    } else if (lightM > darkM) {
        // Overnight light window
        return (nowMin >= lightM || nowMin < darkM);
    }
    return TRUE;
}

BOOL IsStartupEnabled(void) {
    HKEY hKey = NULL;
    BOOL enabled = FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0, sz = 0;
        if (RegQueryValueExW(hKey, REG_RUN_VAL, NULL, &type, NULL, &sz) == ERROR_SUCCESS && sz > 0) {
            enabled = TRUE;
        }
        RegCloseKey(hKey);
    }
    return enabled;
}

BOOL SetStartupEnabled(BOOL enable) {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return FALSE;
    }

    BOOL ok = FALSE;
    if (enable) {
        wchar_t exePath[MAX_PATH];
        DWORD len = GetModuleFileNameW(NULL, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            wchar_t cmd[MAX_PATH + 16];
            _snwprintf(cmd, MAX_PATH + 16, L"\"%s\" --tray", exePath);
            cmd[MAX_PATH + 15] = L'\0';
            DWORD bytes = (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t));
            ok = (RegSetValueExW(hKey, REG_RUN_VAL, 0, REG_SZ, (const BYTE *)cmd, bytes) == ERROR_SUCCESS);
        }
    } else {
        LONG res = RegDeleteValueW(hKey, REG_RUN_VAL);
        ok = (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(hKey);
    return ok;
}
