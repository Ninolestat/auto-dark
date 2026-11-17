#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <stdio.h>
#include "sun.h"
#include "theme.h"
#include "icon.h"

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

#define WM_TRAYICON       (WM_APP + 1)
#define WM_SHOW_POPUP     (WM_APP + 2)
#define TIMER_SCHEDULE    1
#define ID_TRAY_ICON      100

// Context menu IDs
#define IDM_OPEN          1001
#define IDM_LIGHT_NOW     1002
#define IDM_DARK_NOW      1003
#define IDM_TOGGLE_AUTO   1004
#define IDM_TOGGLE_START  1005
#define IDM_EXIT          1006
#define IDM_CITY_BASE     2000

// Hit-test regions
typedef enum {
    HIT_NONE = 0,
    HIT_CLOSE,
    HIT_BTN_LIGHT,
    HIT_BTN_DARK,
    HIT_TOGGLE_AUTO,
    HIT_SEG_SUN,
    HIT_SEG_FIXED,
    HIT_CITY_PREV,
    HIT_CITY_PICK,
    HIT_CITY_NEXT,
    HIT_LIGHT_MINUS,
    HIT_LIGHT_PLUS,
    HIT_DARK_MINUS,
    HIT_DARK_PLUS,
    HIT_TOGGLE_WALL,
    HIT_WALL_LIGHT,
    HIT_WALL_DARK,
    HIT_TOGGLE_STARTUP,
    HIT_EXIT
} HitRegion;

static const wchar_t *WND_CLASS = L"AutoDarkPopupClass";
static const wchar_t *MUTEX_NAME = L"Local\\AutoDark_SingleInstance_Mutex";

static HWND           g_hwnd = NULL;
static HICON          g_trayIcon = NULL;
static HICON          g_appIcon = NULL;
static NOTIFYICONDATAW g_nid;
static AppConfig      g_cfg;
static BOOL           g_startup = FALSE;
static BOOL           g_curLight = TRUE;
static int            g_lastSchedLight = -1; // -1 = unknown, 0 = dark, 1 = light
static int            g_calcSunrise = 420;
static int            g_calcSunset  = 1140;
static UINT           g_dpi = 96;
static HitRegion      g_hover = HIT_NONE;
static UINT           g_msgTaskbarCreated = 0;
static BOOL           g_inDialog = FALSE;

// Logical dimensions (96 DPI)
static const int WIN_W = 320;
static const int WIN_H = 434;

static int S(int px) {
    return MulDiv(px, (int)g_dpi, 96);
}

static void TrimMemory(void) {
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

static void UpdateTrayTooltip(void) {
    wchar_t tip[128];
    if (g_cfg.enabled) {
        if (g_cfg.mode == MODE_SUN) {
            _snwprintf(tip, 128, L"AutoDark \u2022 %s\nAlba %02d:%02d \u2022 Tramonto %02d:%02d",
                       g_curLight ? L"Chiaro" : L"Scuro",
                       g_calcSunrise / 60, g_calcSunrise % 60,
                       g_calcSunset / 60, g_calcSunset % 60);
        } else {
            _snwprintf(tip, 128, L"AutoDark \u2022 %s\nChiaro %02d:%02d \u2022 Scuro %02d:%02d",
                       g_curLight ? L"Chiaro" : L"Scuro",
                       g_cfg.lightHour, g_cfg.lightMin,
                       g_cfg.darkHour, g_cfg.darkMin);
        }
    } else {
        _snwprintf(tip, 128, L"AutoDark \u2022 %s (Manuale)", g_curLight ? L"Chiaro" : L"Scuro");
    }
    tip[127] = L'\0';
    wcsncpy(g_nid.szTip, tip, 127);
    g_nid.szTip[127] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void RefreshTrayIcon(void) {
    HICON newIcon = CreateAutoDarkIcon(GetSystemMetrics(SM_CXSMICON), g_curLight);
    if (newIcon) {
        if (g_trayIcon) DestroyIcon(g_trayIcon);
        g_trayIcon = newIcon;
        g_nid.hIcon = g_trayIcon;
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    }
}

static void EvaluateSchedule(BOOL forceApply) {
    g_curLight = IsSystemLightTheme();
    int lMin = 420, dMin = 1140;
    BOOL shouldLight = ShouldBeLightNow(&g_cfg, &lMin, &dMin);
    g_calcSunrise = lMin;
    g_calcSunset  = dMin;

    if (g_cfg.enabled) {
        int schedState = shouldLight ? 1 : 0;
        if (forceApply || g_lastSchedLight == -1 || g_lastSchedLight != schedState) {
            g_lastSchedLight = schedState;
            if (shouldLight != g_curLight) {
                ApplyWindowsTheme(shouldLight, &g_cfg);
                g_curLight = shouldLight;
                RefreshTrayIcon();
            }
        }
    }
    UpdateTrayTooltip();
}

static void PositionWindowNearTray(HWND hwnd) {
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int w = S(WIN_W);
    int h = S(WIN_H);
    int margin = S(12);
    int x = wa.right - w - margin;
    int y = wa.bottom - h - margin;
    if (x < wa.left + margin) x = wa.left + margin;
    if (y < wa.top + margin)  y = wa.top + margin;
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
}

static void ShowPopup(void) {
    g_curLight = IsSystemLightTheme();
    g_startup  = IsStartupEnabled();
    EvaluateSchedule(FALSE);
    PositionWindowNearTray(g_hwnd);
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void HidePopup(void) {
    ShowWindow(g_hwnd, SW_HIDE);
    TrimMemory();
}

// Compute layout rectangles in client coordinates
static void GetLayoutRects(RECT *rClose, RECT *rBtnLight, RECT *rBtnDark,
                           RECT *rTogAuto, RECT *rSegSun, RECT *rSegFixed,
                           RECT *rCityPrev, RECT *rCityPick, RECT *rCityNext,
                           RECT *rLightMinus, RECT *rLightPlus,
                           RECT *rDarkMinus, RECT *rDarkPlus,
                           RECT *rTogWall, RECT *rWallLight, RECT *rWallDark,
                           RECT *rTogStartup, RECT *rExit) {
    int pad = S(16);
    int w   = S(WIN_W);

    SetRect(rClose, w - S(38), S(10), w - S(10), S(38));

    int btnW = (w - pad * 2 - S(8)) / 2;
    SetRect(rBtnLight, pad, S(54), pad + btnW, S(92));
    SetRect(rBtnDark,  pad + btnW + S(8), S(54), w - pad, S(92));

    // Card 1 interior: x from pad to w - pad, y from 106 to 288
    int cPad = pad + S(12);
    int cRight = w - pad - S(12);

    SetRect(rTogAuto, cRight - S(40), S(120), cRight, S(142));

    int segW = (cRight - cPad - S(4)) / 2;
    SetRect(rSegSun,   cPad + S(2), S(156), cPad + S(2) + segW, S(186));
    SetRect(rSegFixed, cPad + S(2) + segW, S(156), cRight - S(2), S(186));

    // Mode 0: Sun (City picker)
    SetRect(rCityPrev, cPad, S(202), cPad + S(28), S(232));
    SetRect(rCityPick, cPad + S(32), S(202), cRight - S(32), S(232));
    SetRect(rCityNext, cRight - S(28), S(202), cRight, S(232));

    // Mode 1: Fixed (Steppers)
    SetRect(rLightMinus, cRight - S(106), S(202), cRight - S(80), S(228));
    SetRect(rLightPlus,  cRight - S(26),  S(202), cRight,         S(228));
    SetRect(rDarkMinus,  cRight - S(106), S(240), cRight - S(80), S(266));
    SetRect(rDarkPlus,   cRight - S(26),  S(240), cRight,         S(266));

    // Card 2 (Wallpaper): y from 298 to 386
    SetRect(rTogWall,   cRight - S(40), S(308), cRight, S(330));
    int wpW = (cRight - cPad - S(8)) / 2;
    SetRect(rWallLight, cPad, S(342), cPad + wpW, S(374));
    SetRect(rWallDark,  cPad + wpW + S(8), S(342), cRight, S(374));

    // Footer
    SetRect(rTogStartup, pad, S(398), w - pad - S(56), S(424));
    SetRect(rExit,       w - pad - S(48), S(398), w - pad, S(424));
}

static HitRegion HitTestPoint(POINT pt) {
    RECT rClose, rBtnLight, rBtnDark, rTogAuto, rSegSun, rSegFixed;
    RECT rCityPrev, rCityPick, rCityNext;
    RECT rLightMinus, rLightPlus, rDarkMinus, rDarkPlus;
    RECT rTogWall, rWallLight, rWallDark;
    RECT rTogStartup, rExit;

    GetLayoutRects(&rClose, &rBtnLight, &rBtnDark, &rTogAuto, &rSegSun, &rSegFixed,
                   &rCityPrev, &rCityPick, &rCityNext,
                   &rLightMinus, &rLightPlus, &rDarkMinus, &rDarkPlus,
                   &rTogWall, &rWallLight, &rWallDark,
                   &rTogStartup, &rExit);

    if (PtInRect(&rClose, pt))      return HIT_CLOSE;
    if (PtInRect(&rBtnLight, pt))   return HIT_BTN_LIGHT;
    if (PtInRect(&rBtnDark, pt))    return HIT_BTN_DARK;
    if (PtInRect(&rTogAuto, pt))    return HIT_TOGGLE_AUTO;
    if (PtInRect(&rSegSun, pt))     return HIT_SEG_SUN;
    if (PtInRect(&rSegFixed, pt))   return HIT_SEG_FIXED;

    if (g_cfg.mode == MODE_SUN) {
        if (PtInRect(&rCityPrev, pt)) return HIT_CITY_PREV;
        if (PtInRect(&rCityPick, pt)) return HIT_CITY_PICK;
        if (PtInRect(&rCityNext, pt)) return HIT_CITY_NEXT;
    } else {
        if (PtInRect(&rLightMinus, pt)) return HIT_LIGHT_MINUS;
        if (PtInRect(&rLightPlus, pt))  return HIT_LIGHT_PLUS;
        if (PtInRect(&rDarkMinus, pt))  return HIT_DARK_MINUS;
        if (PtInRect(&rDarkPlus, pt))   return HIT_DARK_PLUS;
    }

    if (PtInRect(&rTogWall, pt))    return HIT_TOGGLE_WALL;
    if (PtInRect(&rWallLight, pt))  return HIT_WALL_LIGHT;
    if (PtInRect(&rWallDark, pt))   return HIT_WALL_DARK;
    if (PtInRect(&rTogStartup, pt)) return HIT_TOGGLE_STARTUP;
    if (PtInRect(&rExit, pt))       return HIT_EXIT;
    return HIT_NONE;
}

static void FillRoundRect(HDC hdc, const RECT *rc, int radius, COLORREF fill, COLORREF border) {
    HBRUSH hBr = CreateSolidBrush(fill);
    HPEN   hPen = (border != CLR_INVALID) ? CreatePen(PS_SOLID, 1, border) : (HPEN)GetStockObject(NULL_PEN);
    HGDIOBJ oldBr  = SelectObject(hdc, hBr);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius, radius);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    if (border != CLR_INVALID) DeleteObject(hPen);
}

static HFONT MakeFont(int ptSize, int weight) {
    return CreateFontW(-MulDiv(ptSize, (int)g_dpi, 72), 0, 0, 0, weight,
                       FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

static void DrawToggleSwitch(HDC hdc, const RECT *rc, BOOL on, BOOL hover, BOOL darkUI) {
    int h = rc->bottom - rc->top;
    int w = rc->right - rc->left;
    COLORREF track = on
        ? (hover ? RGB(59, 130, 246) : RGB(37, 99, 235))
        : (darkUI ? (hover ? RGB(72, 78, 92) : RGB(56, 61, 74)) : (hover ? RGB(190, 196, 206) : RGB(209, 213, 219)));
    FillRoundRect(hdc, rc, h, track, CLR_INVALID);

    int pad = S(3);
    int knobD = h - pad * 2;
    int kx = on ? (rc->right - pad - knobD) : (rc->left + pad);
    int ky = rc->top + pad;
    RECT rk = { kx, ky, kx + knobD, ky + knobD };
    FillRoundRect(hdc, &rk, knobD, RGB(255, 255, 255), CLR_INVALID);
    (void)w;
}

static const wchar_t *FileNameFromPath(const wchar_t *path) {
    if (!path || !path[0]) return L"Scegli...";
    const wchar_t *slash = wcsrchr(path, L'\\');
    if (!slash) slash = wcsrchr(path, L'/');
    return slash ? (slash + 1) : path;
}

static void PaintWindow(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdcWin = BeginPaint(hwnd, &ps);

    int w = S(WIN_W);
    int h = S(WIN_H);
    HDC hdc = CreateCompatibleDC(hdcWin);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcWin, w, h);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

    BOOL dark = !g_curLight;

    // Adaptive palette
    COLORREF cBg       = dark ? RGB(24, 26, 32)    : RGB(246, 247, 250);
    COLORREF cCard     = dark ? RGB(33, 36, 45)    : RGB(255, 255, 255);
    COLORREF cBorder   = dark ? RGB(52, 56, 68)    : RGB(226, 230, 238);
    COLORREF cText     = dark ? RGB(240, 242, 246) : RGB(24, 27, 34);
    COLORREF cMuted    = dark ? RGB(148, 156, 172) : RGB(107, 114, 128);
    COLORREF cBtnBg    = dark ? RGB(38, 42, 52)    : RGB(255, 255, 255);
    COLORREF cBtnHover = dark ? RGB(48, 53, 66)    : RGB(241, 244, 249);
    COLORREF cSegBg    = dark ? RGB(24, 26, 32)    : RGB(238, 240, 245);
    COLORREF cAccent   = dark ? RGB(96, 165, 250)  : RGB(37, 99, 235);
    COLORREF cSun      = RGB(245, 158, 11);

    RECT rFull = { 0, 0, w, h };
    HBRUSH hBg = CreateSolidBrush(cBg);
    FillRect(hdc, &rFull, hBg);
    DeleteObject(hBg);

    // Subtle outer border
    HPEN hOutPen = CreatePen(PS_SOLID, 1, cBorder);
    HGDIOBJ oldPen = SelectObject(hdc, hOutPen);
    HGDIOBJ oldBr  = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, w, h);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hOutPen);

    SetBkMode(hdc, TRANSPARENT);

    HFONT fTitle = MakeFont(12, FW_SEMIBOLD);
    HFONT fBody  = MakeFont(9,  FW_NORMAL);
    HFONT fBold  = MakeFont(9,  FW_SEMIBOLD);
    HFONT fSmall = MakeFont(8,  FW_NORMAL);

    RECT rClose, rBtnLight, rBtnDark, rTogAuto, rSegSun, rSegFixed;
    RECT rCityPrev, rCityPick, rCityNext;
    RECT rLightMinus, rLightPlus, rDarkMinus, rDarkPlus;
    RECT rTogWall, rWallLight, rWallDark;
    RECT rTogStartup, rExit;

    GetLayoutRects(&rClose, &rBtnLight, &rBtnDark, &rTogAuto, &rSegSun, &rSegFixed,
                   &rCityPrev, &rCityPick, &rCityNext,
                   &rLightMinus, &rLightPlus, &rDarkMinus, &rDarkPlus,
                   &rTogWall, &rWallLight, &rWallDark,
                   &rTogStartup, &rExit);

    // 1. Header
    if (g_appIcon) {
        DrawIconEx(hdc, S(16), S(14), g_appIcon, S(20), S(20), 0, NULL, DI_NORMAL);
    }
    SelectObject(hdc, fTitle);
    SetTextColor(hdc, cText);
    RECT rTitle = { S(42), S(13), S(150), S(37) };
    DrawTextW(hdc, L"AutoDark", -1, &rTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Status pill
    RECT rPill = { S(122), S(15), S(182), S(35) };
    COLORREF pillBg = g_curLight ? RGB(254, 243, 199) : RGB(49, 46, 129);
    COLORREF pillFg = g_curLight ? RGB(180, 83, 9)    : RGB(199, 210, 254);
    FillRoundRect(hdc, &rPill, S(18), pillBg, CLR_INVALID);
    SelectObject(hdc, fSmall);
    SetTextColor(hdc, pillFg);
    DrawTextW(hdc, g_curLight ? L"Chiaro" : L"Scuro", -1, &rPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Close button
    if (g_hover == HIT_CLOSE) {
        FillRoundRect(hdc, &rClose, S(8), cBtnHover, CLR_INVALID);
    }
    SelectObject(hdc, fTitle);
    SetTextColor(hdc, cMuted);
    DrawTextW(hdc, L"\u00D7", -1, &rClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 2. Quick Theme Buttons
    BOOL lightActive = g_curLight;
    COLORREF blFill = (g_hover == HIT_BTN_LIGHT) ? cBtnHover : cBtnBg;
    COLORREF bdFill = (g_hover == HIT_BTN_DARK)  ? cBtnHover : cBtnBg;
    FillRoundRect(hdc, &rBtnLight, S(10), blFill, lightActive ? cSun : cBorder);
    FillRoundRect(hdc, &rBtnDark,  S(10), bdFill, !lightActive ? cAccent : cBorder);

    SelectObject(hdc, lightActive ? fBold : fBody);
    SetTextColor(hdc, lightActive ? (dark ? RGB(253, 224, 71) : RGB(180, 83, 9)) : cText);
    DrawTextW(hdc, L"\u2600  Chiaro", -1, &rBtnLight, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, !lightActive ? fBold : fBody);
    SetTextColor(hdc, !lightActive ? cAccent : cText);
    DrawTextW(hdc, L"\u263E  Scuro", -1, &rBtnDark, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 3. Main Schedule Card
    int pad = S(16);
    RECT rCard = { pad, S(106), w - pad, S(288) };
    FillRoundRect(hdc, &rCard, S(12), cCard, cBorder);

    // Auto toggle row
    SelectObject(hdc, fBold);
    SetTextColor(hdc, cText);
    RECT rAutoLbl = { pad + S(12), S(118), w - pad - S(60), S(144) };
    DrawTextW(hdc, L"Cambio automatico", -1, &rAutoLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawToggleSwitch(hdc, &rTogAuto, g_cfg.enabled, g_hover == HIT_TOGGLE_AUTO, dark);

    // Segmented control
    RECT rSegOuter = { pad + S(12), S(154), w - pad - S(12), S(188) };
    FillRoundRect(hdc, &rSegOuter, S(10), cSegBg, CLR_INVALID);

    if (g_cfg.mode == MODE_SUN) {
        FillRoundRect(hdc, &rSegSun, S(8), cCard, cBorder);
    } else {
        FillRoundRect(hdc, &rSegFixed, S(8), cCard, cBorder);
    }

    SelectObject(hdc, g_cfg.mode == MODE_SUN ? fBold : fBody);
    SetTextColor(hdc, g_cfg.mode == MODE_SUN ? cText : cMuted);
    DrawTextW(hdc, L"Alba / Tramonto", -1, &rSegSun, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, g_cfg.mode == MODE_FIXED ? fBold : fBody);
    SetTextColor(hdc, g_cfg.mode == MODE_FIXED ? cText : cMuted);
    DrawTextW(hdc, L"Orario fisso", -1, &rSegFixed, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Contextual body
    if (g_cfg.mode == MODE_SUN) {
        FillRoundRect(hdc, &rCityPrev, S(8), g_hover == HIT_CITY_PREV ? cBtnHover : cSegBg, CLR_INVALID);
        FillRoundRect(hdc, &rCityPick, S(8), g_hover == HIT_CITY_PICK ? cBtnHover : cSegBg, CLR_INVALID);
        FillRoundRect(hdc, &rCityNext, S(8), g_hover == HIT_CITY_NEXT ? cBtnHover : cSegBg, CLR_INVALID);

        SelectObject(hdc, fBold);
        SetTextColor(hdc, cMuted);
        DrawTextW(hdc, L"\u2039", -1, &rCityPrev, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(hdc, L"\u203A", -1, &rCityNext, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SetTextColor(hdc, cText);
        const wchar_t *cityName = (g_cfg.cityIndex >= 0 && g_cfg.cityIndex < g_cityCount)
            ? g_cities[g_cfg.cityIndex].name : L"Personalizzata";
        DrawTextW(hdc, cityName, -1, &rCityPick, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Sunrise / Sunset summary
        RECT rSunInfo = { pad + S(12), S(244), w - pad - S(12), S(274) };
        wchar_t buf[96];
        _snwprintf(buf, 96, L"\u2600 Alba %02d:%02d     \u2022     \u263E Tramonto %02d:%02d",
                   g_calcSunrise / 60, g_calcSunrise % 60,
                   g_calcSunset / 60, g_calcSunset % 60);
        SelectObject(hdc, fBody);
        SetTextColor(hdc, cMuted);
        DrawTextW(hdc, buf, -1, &rSunInfo, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        // Fixed schedule rows
        RECT rL1 = { pad + S(12), S(202), w - pad - S(120), S(228) };
        RECT rL2 = { pad + S(12), S(240), w - pad - S(120), S(266) };
        SelectObject(hdc, fBody);
        SetTextColor(hdc, cText);
        DrawTextW(hdc, L"\u2600  Chiaro alle", -1, &rL1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(hdc, L"\u263E  Scuro alle",  -1, &rL2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        FillRoundRect(hdc, &rLightMinus, S(6), g_hover == HIT_LIGHT_MINUS ? cBtnHover : cSegBg, CLR_INVALID);
        FillRoundRect(hdc, &rLightPlus,  S(6), g_hover == HIT_LIGHT_PLUS  ? cBtnHover : cSegBg, CLR_INVALID);
        FillRoundRect(hdc, &rDarkMinus,  S(6), g_hover == HIT_DARK_MINUS  ? cBtnHover : cSegBg, CLR_INVALID);
        FillRoundRect(hdc, &rDarkPlus,   S(6), g_hover == HIT_DARK_PLUS   ? cBtnHover : cSegBg, CLR_INVALID);

        SelectObject(hdc, fBold);
        SetTextColor(hdc, cText);
        DrawTextW(hdc, L"-", -1, &rLightMinus, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(hdc, L"+", -1, &rLightPlus,  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(hdc, L"-", -1, &rDarkMinus,  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(hdc, L"+", -1, &rDarkPlus,   DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        wchar_t t1[16], t2[16];
        _snwprintf(t1, 16, L"%02d:%02d", g_cfg.lightHour, g_cfg.lightMin);
        _snwprintf(t2, 16, L"%02d:%02d", g_cfg.darkHour,  g_cfg.darkMin);
        RECT rT1 = { rLightMinus.right, S(202), rLightPlus.left, S(228) };
        RECT rT2 = { rDarkMinus.right,  S(240), rDarkPlus.left,  S(266) };
        DrawTextW(hdc, t1, -1, &rT1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(hdc, t2, -1, &rT2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 4. Wallpaper Card
    RECT rWallCard = { pad, S(298), w - pad, S(386) };
    FillRoundRect(hdc, &rWallCard, S(12), cCard, cBorder);

    SelectObject(hdc, fBold);
    SetTextColor(hdc, cText);
    RECT rWallLbl = { pad + S(12), S(306), w - pad - S(60), S(332) };
    DrawTextW(hdc, L"Cambia sfondo", -1, &rWallLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawToggleSwitch(hdc, &rTogWall, g_cfg.changeWallpaper, g_hover == HIT_TOGGLE_WALL, dark);

    FillRoundRect(hdc, &rWallLight, S(8), g_hover == HIT_WALL_LIGHT ? cBtnHover : cSegBg, cBorder);
    FillRoundRect(hdc, &rWallDark,  S(8), g_hover == HIT_WALL_DARK  ? cBtnHover : cSegBg, cBorder);

    wchar_t wlBuf[MAX_PATH + 16], wdBuf[MAX_PATH + 16];
    _snwprintf(wlBuf, MAX_PATH + 16, L"\u2600 %s", FileNameFromPath(g_cfg.lightWallpaper));
    _snwprintf(wdBuf, MAX_PATH + 16, L"\u263E %s", FileNameFromPath(g_cfg.darkWallpaper));
    wlBuf[MAX_PATH + 15] = L'\0';
    wdBuf[MAX_PATH + 15] = L'\0';

    SelectObject(hdc, fSmall);
    SetTextColor(hdc, g_cfg.changeWallpaper ? cText : cMuted);
    RECT rWLT = rWallLight; rWLT.left += S(6); rWLT.right -= S(6);
    RECT rWDT = rWallDark;  rWDT.left += S(6); rWDT.right -= S(6);
    DrawTextW(hdc, wlBuf, -1, &rWLT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    DrawTextW(hdc, wdBuf, -1, &rWDT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // 5. Footer: Startup checkbox + Exit
    RECT rChk = { rTogStartup.left, rTogStartup.top + S(5), rTogStartup.left + S(16), rTogStartup.top + S(21) };
    FillRoundRect(hdc, &rChk, S(4),
                  g_startup ? cAccent : (g_hover == HIT_TOGGLE_STARTUP ? cBtnHover : cCard),
                  g_startup ? cAccent : cBorder);
    if (g_startup) {
        SelectObject(hdc, fSmall);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, L"\u2713", -1, &rChk, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    RECT rStartLbl = { rChk.right + S(8), rTogStartup.top, rTogStartup.right, rTogStartup.bottom };
    SelectObject(hdc, fBody);
    SetTextColor(hdc, g_hover == HIT_TOGGLE_STARTUP ? cText : cMuted);
    DrawTextW(hdc, L"Avvia con Windows", -1, &rStartLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (g_hover == HIT_EXIT) {
        FillRoundRect(hdc, &rExit, S(6), cBtnHover, CLR_INVALID);
    }
    SetTextColor(hdc, g_hover == HIT_EXIT ? RGB(239, 68, 68) : cMuted);
    DrawTextW(hdc, L"Esci", -1, &rExit, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    BitBlt(hdcWin, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

    DeleteObject(fTitle);
    DeleteObject(fBody);
    DeleteObject(fBold);
    DeleteObject(fSmall);
    SelectObject(hdc, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    EndPaint(hwnd, &ps);
}

static void AdjustTime(int *hour, int *min, int deltaMin) {
    int total = (*hour) * 60 + (*min) + deltaMin;
    while (total < 0) total += 1440;
    while (total >= 1440) total -= 1440;
    *hour = total / 60;
    *min  = total % 60;
}

static void PickWallpaperFile(HWND hwnd, wchar_t *pathBuf, DWORD cap) {
    wchar_t fileBuf[MAX_PATH];
    wcsncpy(fileBuf, pathBuf, MAX_PATH - 1);
    fileBuf[MAX_PATH - 1] = L'\0';

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = hwnd;
    ofn.lpstrFilter     = L"Immagini (*.jpg;*.jpeg;*.png;*.bmp)\0*.jpg;*.jpeg;*.png;*.bmp\0Tutti i file (*.*)\0*.*\0";
    ofn.lpstrFile       = fileBuf;
    ofn.nMaxFile        = MAX_PATH;
    ofn.lpstrInitialDir = L"C:\\Windows\\Web\\Wallpaper\\Windows";
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    g_inDialog = TRUE;
    BOOL ok = GetOpenFileNameW(&ofn);
    g_inDialog = FALSE;

    if (ok) {
        wcsncpy(pathBuf, fileBuf, cap - 1);
        pathBuf[cap - 1] = L'\0';
    }
    SetForegroundWindow(hwnd);
}

static void ShowCityMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    for (int i = 0; i < g_cityCount; i++) {
        UINT flags = MF_STRING | ((g_cfg.cityIndex == i) ? MF_CHECKED : 0);
        AppendMenuW(hMenu, flags, (UINT_PTR)(IDM_CITY_BASE + i), g_cities[i].name);
    }
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);

    if (cmd >= IDM_CITY_BASE && cmd < IDM_CITY_BASE + g_cityCount) {
        int idx = cmd - IDM_CITY_BASE;
        g_cfg.cityIndex = idx;
        g_cfg.lat = g_cities[idx].lat;
        g_cfg.lon = g_cities[idx].lon;
        ConfigSave(&g_cfg);
        EvaluateSchedule(TRUE);
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

static void ShowTrayMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN, L"Apri AutoDark");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (g_curLight ? MF_CHECKED : 0), IDM_LIGHT_NOW, L"Tema Chiaro");
    AppendMenuW(hMenu, MF_STRING | (!g_curLight ? MF_CHECKED : 0), IDM_DARK_NOW, L"Tema Scuro");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (g_cfg.enabled ? MF_CHECKED : 0), IDM_TOGGLE_AUTO, L"Cambio automatico");
    AppendMenuW(hMenu, MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0), IDM_TOGGLE_START, L"Avvia con Windows");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Esci");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

static void AddTrayIcon(HWND hwnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = ID_TRAY_ICON;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_trayIcon;
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    UpdateTrayTooltip();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == g_msgTaskbarCreated && g_msgTaskbarCreated != 0) {
        AddTrayIcon(hwnd);
        return 0;
    }

    switch (msg) {
    case WM_CREATE: {
        int corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        g_curLight = IsSystemLightTheme();
        g_startup  = IsStartupEnabled();
        g_trayIcon = CreateAutoDarkIcon(GetSystemMetrics(SM_CXSMICON), g_curLight);
        g_appIcon  = CreateAutoDarkIcon(S(32), g_curLight);
        AddTrayIcon(hwnd);
        SetTimer(hwnd, TIMER_SCHEDULE, 60000, NULL);
        EvaluateSchedule(TRUE);
        return 0;
    }

    case WM_TIMER:
        if (wParam == TIMER_SCHEDULE) {
            EvaluateSchedule(FALSE);
            if (IsWindowVisible(hwnd)) InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_TIMECHANGE:
        EvaluateSchedule(TRUE);
        if (IsWindowVisible(hwnd)) InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            EvaluateSchedule(TRUE);
        }
        return TRUE;

    case WM_SETTINGCHANGE:
        if (lParam && wcscmp((const wchar_t *)lParam, L"ImmersiveColorSet") == 0) {
            BOOL sysLight = IsSystemLightTheme();
            if (sysLight != g_curLight) {
                g_curLight = sysLight;
                RefreshTrayIcon();
                UpdateTrayTooltip();
                if (IsWindowVisible(hwnd)) InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;

    case WM_SHOW_POPUP:
        ShowPopup();
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
            if (IsWindowVisible(hwnd)) HidePopup();
            else ShowPopup();
        } else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            ShowTrayMenu(hwnd);
        }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !g_inDialog && IsWindowVisible(hwnd)) {
            HidePopup();
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            HidePopup();
            return 0;
        }
        break;

    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        HitRegion h = HitTestPoint(pt);
        if (h != g_hover) {
            g_hover = h;
            InvalidateRect(hwnd, NULL, FALSE);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_hover != HIT_NONE) {
            g_hover = HIT_NONE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONUP: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        HitRegion h = HitTestPoint(pt);
        switch (h) {
        case HIT_CLOSE:
            HidePopup();
            break;
        case HIT_BTN_LIGHT:
            ApplyWindowsTheme(TRUE, &g_cfg);
            g_curLight = TRUE;
            RefreshTrayIcon();
            UpdateTrayTooltip();
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_BTN_DARK:
            ApplyWindowsTheme(FALSE, &g_cfg);
            g_curLight = FALSE;
            RefreshTrayIcon();
            UpdateTrayTooltip();
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_TOGGLE_AUTO:
            g_cfg.enabled = !g_cfg.enabled;
            ConfigSave(&g_cfg);
            if (g_cfg.enabled) EvaluateSchedule(TRUE);
            else UpdateTrayTooltip();
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_SEG_SUN:
            if (g_cfg.mode != MODE_SUN) {
                g_cfg.mode = MODE_SUN;
                ConfigSave(&g_cfg);
                EvaluateSchedule(TRUE);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case HIT_SEG_FIXED:
            if (g_cfg.mode != MODE_FIXED) {
                g_cfg.mode = MODE_FIXED;
                ConfigSave(&g_cfg);
                EvaluateSchedule(TRUE);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case HIT_CITY_PREV:
            g_cfg.cityIndex = (g_cfg.cityIndex - 1 + g_cityCount) % g_cityCount;
            g_cfg.lat = g_cities[g_cfg.cityIndex].lat;
            g_cfg.lon = g_cities[g_cfg.cityIndex].lon;
            ConfigSave(&g_cfg);
            EvaluateSchedule(TRUE);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_CITY_NEXT:
            g_cfg.cityIndex = (g_cfg.cityIndex + 1) % g_cityCount;
            g_cfg.lat = g_cities[g_cfg.cityIndex].lat;
            g_cfg.lon = g_cities[g_cfg.cityIndex].lon;
            ConfigSave(&g_cfg);
            EvaluateSchedule(TRUE);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_CITY_PICK:
            ShowCityMenu(hwnd);
            break;
        case HIT_LIGHT_MINUS:
            AdjustTime(&g_cfg.lightHour, &g_cfg.lightMin, -15);
            ConfigSave(&g_cfg);
            EvaluateSchedule(TRUE);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_LIGHT_PLUS:
            AdjustTime(&g_cfg.lightHour, &g_cfg.lightMin, 15);
            ConfigSave(&g_cfg);
            EvaluateSchedule(TRUE);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_DARK_MINUS:
            AdjustTime(&g_cfg.darkHour, &g_cfg.darkMin, -15);
            ConfigSave(&g_cfg);
            EvaluateSchedule(TRUE);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_DARK_PLUS:
            AdjustTime(&g_cfg.darkHour, &g_cfg.darkMin, 15);
            ConfigSave(&g_cfg);
            EvaluateSchedule(TRUE);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_TOGGLE_WALL:
            g_cfg.changeWallpaper = !g_cfg.changeWallpaper;
            ConfigSave(&g_cfg);
            if (g_cfg.changeWallpaper) ApplyWindowsTheme(g_curLight, &g_cfg);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_WALL_LIGHT:
            PickWallpaperFile(hwnd, g_cfg.lightWallpaper, MAX_PATH);
            g_cfg.changeWallpaper = TRUE;
            ConfigSave(&g_cfg);
            if (g_curLight) ApplyWindowsTheme(TRUE, &g_cfg);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_WALL_DARK:
            PickWallpaperFile(hwnd, g_cfg.darkWallpaper, MAX_PATH);
            g_cfg.changeWallpaper = TRUE;
            ConfigSave(&g_cfg);
            if (!g_curLight) ApplyWindowsTheme(FALSE, &g_cfg);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_TOGGLE_STARTUP:
            g_startup = !g_startup;
            SetStartupEnabled(g_startup);
            g_startup = IsStartupEnabled();
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case HIT_EXIT:
            DestroyWindow(hwnd);
            break;
        default:
            break;
        }
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_OPEN:
            ShowPopup();
            break;
        case IDM_LIGHT_NOW:
            ApplyWindowsTheme(TRUE, &g_cfg);
            g_curLight = TRUE;
            RefreshTrayIcon();
            UpdateTrayTooltip();
            break;
        case IDM_DARK_NOW:
            ApplyWindowsTheme(FALSE, &g_cfg);
            g_curLight = FALSE;
            RefreshTrayIcon();
            UpdateTrayTooltip();
            break;
        case IDM_TOGGLE_AUTO:
            g_cfg.enabled = !g_cfg.enabled;
            ConfigSave(&g_cfg);
            if (g_cfg.enabled) EvaluateSchedule(TRUE);
            else UpdateTrayTooltip();
            if (IsWindowVisible(hwnd)) InvalidateRect(hwnd, NULL, FALSE);
            break;
        case IDM_TOGGLE_START:
            g_startup = !IsStartupEnabled();
            SetStartupEnabled(g_startup);
            if (IsWindowVisible(hwnd)) InvalidateRect(hwnd, NULL, FALSE);
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_PAINT:
        PaintWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_SCHEDULE);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        if (g_trayIcon) DestroyIcon(g_trayIcon);
        if (g_appIcon)  DestroyIcon(g_appIcon);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmdLine, int nShow) {
    (void)hPrev; (void)nShow;

    BOOL startInTray = (lpCmdLine && wcsstr(lpCmdLine, L"--tray") != NULL);

    HANDLE hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(WND_CLASS, NULL);
        if (existing && !startInTray) {
            PostMessageW(existing, WM_SHOW_POPUP, 0, 0);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HDC hdcScreen = GetDC(NULL);
    g_dpi = (UINT)GetDeviceCaps(hdcScreen, LOGPIXELSX);
    ReleaseDC(NULL, hdcScreen);

    ConfigLoad(&g_cfg);
    g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = WND_CLASS;
    RegisterClassExW(&wc);

    // WS_EX_TOOLWINDOW keeps it out of Alt+Tab and Taskbar
    g_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        WND_CLASS, L"AutoDark",
        WS_POPUP,
        0, 0, S(WIN_W), S(WIN_H),
        NULL, NULL, hInst, NULL);

    if (!startInTray) {
        ShowPopup();
    } else {
        TrimMemory();
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hMutex) CloseHandle(hMutex);
    return 0;
}
