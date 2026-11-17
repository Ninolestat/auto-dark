#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>
#include <dwmapi.h>
#include <stdio.h>
#include "icon.h"
#include "../build/payload.h"

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

static const wchar_t *APP_WND_CLASS = L"AutoDarkPopupClass";
static const wchar_t *INST_WND_CLASS = L"AutoDarkSetupClass";

static UINT  g_dpi = 96;
static HICON g_icon = NULL;
static BOOL  g_dark = FALSE;
static BOOL  g_chkStartup = TRUE;
static BOOL  g_chkLaunch  = TRUE;
static BOOL  g_done       = FALSE;
static int   g_hover      = 0; // 1=close, 2=chk1, 3=chk2, 4=install, 5=cancel

static const int WIN_W = 400;
static const int WIN_H = 252;

static int S(int px) { return MulDiv(px, (int)g_dpi, 96); }

static BOOL IsWinDark(void) {
    HKEY hKey = NULL;
    DWORD val = 1, sz = sizeof(DWORD);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&val, &sz);
        RegCloseKey(hKey);
    }
    return val == 0;
}

static void StopRunningAutoDark(void) {
    HWND existing = FindWindowW(APP_WND_CLASS, NULL);
    if (existing) {
        PostMessageW(existing, WM_COMMAND, 1006 /* IDM_EXIT */, 0);
        for (int i = 0; i < 20; i++) {
            Sleep(50);
            if (!FindWindowW(APP_WND_CLASS, NULL)) break;
        }
    }
}

static void GetInstallDir(wchar_t *out, DWORD cap) {
    wchar_t localApp[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp))) {
        _snwprintf(out, cap, L"%s\\Programs\\AutoDark", localApp);
        out[cap - 1] = L'\0';
    }
}

static void GetShortcutPath(wchar_t *out, DWORD cap) {
    wchar_t programs[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, programs))) {
        _snwprintf(out, cap, L"%s\\AutoDark.lnk", programs);
        out[cap - 1] = L'\0';
    }
}

static BOOL CreateShortcut(const wchar_t *targetExe, const wchar_t *lnkPath, const wchar_t *workDir) {
    HRESULT hr = CoInitialize(NULL);
    BOOL ok = FALSE;
    IShellLinkW *psl = NULL;
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&psl))) {
        psl->lpVtbl->SetPath(psl, targetExe);
        psl->lpVtbl->SetWorkingDirectory(psl, workDir);
        psl->lpVtbl->SetDescription(psl, L"Cambio automatico tema Chiaro/Scuro");
        IPersistFile *ppf = NULL;
        if (SUCCEEDED(psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)&ppf))) {
            ok = SUCCEEDED(ppf->lpVtbl->Save(ppf, lnkPath, TRUE));
            ppf->lpVtbl->Release(ppf);
        }
        psl->lpVtbl->Release(psl);
    }
    if (SUCCEEDED(hr)) CoUninitialize();
    return ok;
}

static BOOL WriteFileBytes(const wchar_t *path, const unsigned char *data, DWORD size) {
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, size, &written, NULL) && written == size;
    CloseHandle(hFile);
    return ok;
}

static BOOL PerformInstall(void) {
    StopRunningAutoDark();

    wchar_t dir[MAX_PATH], exePath[MAX_PATH], uninstPath[MAX_PATH], selfPath[MAX_PATH], lnkPath[MAX_PATH];
    GetInstallDir(dir, MAX_PATH);
    SHCreateDirectoryExW(NULL, dir, NULL);

    _snwprintf(exePath, MAX_PATH, L"%s\\AutoDark.exe", dir);
    _snwprintf(uninstPath, MAX_PATH, L"%s\\Uninstall.exe", dir);

    if (!WriteFileBytes(exePath, g_payload, (DWORD)g_payload_len)) {
        return FALSE;
    }

    if (GetModuleFileNameW(NULL, selfPath, MAX_PATH) > 0) {
        CopyFileW(selfPath, uninstPath, FALSE);
    }

    GetShortcutPath(lnkPath, MAX_PATH);
    CreateShortcut(exePath, lnkPath, dir);

    // Register Uninstall entry
    HKEY hUninst = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\AutoDark",
                        0, NULL, 0, KEY_WRITE, NULL, &hUninst, NULL) == ERROR_SUCCESS) {
        wchar_t uninstCmd[MAX_PATH + 32];
        _snwprintf(uninstCmd, MAX_PATH + 32, L"\"%s\" --uninstall", uninstPath);
        DWORD one = 1;
        DWORD kb  = (DWORD)((g_payload_len + 1023) / 1024);

        RegSetValueExW(hUninst, L"DisplayName",     0, REG_SZ,    (const BYTE *)L"AutoDark", 18);
        RegSetValueExW(hUninst, L"DisplayVersion",  0, REG_SZ,    (const BYTE *)L"1.0.0", 12);
        RegSetValueExW(hUninst, L"Publisher",       0, REG_SZ,    (const BYTE *)L"AutoDark", 18);
        RegSetValueExW(hUninst, L"DisplayIcon",     0, REG_SZ,    (const BYTE *)exePath, (DWORD)((wcslen(exePath) + 1) * 2));
        RegSetValueExW(hUninst, L"UninstallString", 0, REG_SZ,    (const BYTE *)uninstCmd, (DWORD)((wcslen(uninstCmd) + 1) * 2));
        RegSetValueExW(hUninst, L"InstallLocation", 0, REG_SZ,    (const BYTE *)dir, (DWORD)((wcslen(dir) + 1) * 2));
        RegSetValueExW(hUninst, L"NoModify",        0, REG_DWORD, (const BYTE *)&one, sizeof(DWORD));
        RegSetValueExW(hUninst, L"NoRepair",        0, REG_DWORD, (const BYTE *)&one, sizeof(DWORD));
        RegSetValueExW(hUninst, L"EstimatedSize",   0, REG_DWORD, (const BYTE *)&kb, sizeof(DWORD));
        RegCloseKey(hUninst);
    }

    // Startup registration
    HKEY hRun = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hRun) == ERROR_SUCCESS) {
        if (g_chkStartup) {
            wchar_t runCmd[MAX_PATH + 16];
            _snwprintf(runCmd, MAX_PATH + 16, L"\"%s\" --tray", exePath);
            RegSetValueExW(hRun, L"AutoDark", 0, REG_SZ, (const BYTE *)runCmd, (DWORD)((wcslen(runCmd) + 1) * 2));
        } else {
            RegDeleteValueW(hRun, L"AutoDark");
        }
        RegCloseKey(hRun);
    }

    if (g_chkLaunch) {
        ShellExecuteW(NULL, L"open", exePath, L"--tray", dir, SW_SHOWNORMAL);
    }
    return TRUE;
}

static void PerformUninstall(void) {
    StopRunningAutoDark();

    HKEY hRun = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hRun) == ERROR_SUCCESS) {
        RegDeleteValueW(hRun, L"AutoDark");
        RegCloseKey(hRun);
    }
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\AutoDark");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\AutoDark");

    wchar_t lnkPath[MAX_PATH], dir[MAX_PATH], exePath[MAX_PATH];
    GetShortcutPath(lnkPath, MAX_PATH);
    DeleteFileW(lnkPath);

    GetInstallDir(dir, MAX_PATH);
    _snwprintf(exePath, MAX_PATH, L"%s\\AutoDark.exe", dir);
    DeleteFileW(exePath);

    // Schedule removal of Uninstall.exe & directory after exit
    wchar_t cmd[MAX_PATH * 2];
    _snwprintf(cmd, MAX_PATH * 2, L"/c ping 127.0.0.1 -n 2 > nul & rmdir /s /q \"%s\"", dir);
    ShellExecuteW(NULL, L"open", L"cmd.exe", cmd, NULL, SW_HIDE);
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

static void GetRects(RECT *rClose, RECT *rChk1, RECT *rChk2, RECT *rBtnOk, RECT *rBtnCancel) {
    int w = S(WIN_W), h = S(WIN_H), pad = S(22);
    SetRect(rClose,     w - S(38), S(10), w - S(10), S(38));
    SetRect(rChk1,      pad, S(98),  w - pad, S(128));
    SetRect(rChk2,      pad, S(134), w - pad, S(164));
    SetRect(rBtnCancel, w - pad - S(196), h - S(58), w - pad - S(104), h - S(22));
    SetRect(rBtnOk,     w - pad - S(96),  h - S(58), w - pad,          h - S(22));
}

static int HitTest(POINT pt) {
    RECT rClose, rChk1, rChk2, rOk, rCancel;
    GetRects(&rClose, &rChk1, &rChk2, &rOk, &rCancel);
    if (PtInRect(&rClose, pt)) return 1;
    if (!g_done) {
        if (PtInRect(&rChk1, pt))  return 2;
        if (PtInRect(&rChk2, pt))  return 3;
        if (PtInRect(&rCancel, pt)) return 5;
    }
    if (PtInRect(&rOk, pt)) return 4;
    return 0;
}

static void PaintSetup(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdcWin = BeginPaint(hwnd, &ps);
    int w = S(WIN_W), h = S(WIN_H);

    HDC hdc = CreateCompatibleDC(hdcWin);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcWin, w, h);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

    COLORREF cBg       = g_dark ? RGB(24, 26, 32)    : RGB(248, 249, 252);
    COLORREF cBorder   = g_dark ? RGB(52, 56, 68)    : RGB(224, 228, 236);
    COLORREF cText     = g_dark ? RGB(240, 242, 246) : RGB(24, 27, 34);
    COLORREF cMuted    = g_dark ? RGB(148, 156, 172) : RGB(107, 114, 128);
    COLORREF cBtnHover = g_dark ? RGB(45, 50, 62)    : RGB(236, 240, 247);
    COLORREF cAccent   = g_dark ? RGB(59, 130, 246)  : RGB(37, 99, 235);
    COLORREF cAccHov   = g_dark ? RGB(96, 165, 250)  : RGB(29, 78, 216);

    RECT rFull = { 0, 0, w, h };
    HBRUSH hBg = CreateSolidBrush(cBg);
    FillRect(hdc, &rFull, hBg);
    DeleteObject(hBg);

    HPEN hPen = CreatePen(PS_SOLID, 1, cBorder);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBr  = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, w, h);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);

    SetBkMode(hdc, TRANSPARENT);
    HFONT fTitle = MakeFont(13, FW_SEMIBOLD);
    HFONT fBody  = MakeFont(9,  FW_NORMAL);
    HFONT fBold  = MakeFont(9,  FW_SEMIBOLD);
    HFONT fSmall = MakeFont(8,  FW_NORMAL);

    RECT rClose, rChk1, rChk2, rOk, rCancel;
    GetRects(&rClose, &rChk1, &rChk2, &rOk, &rCancel);

    if (g_icon) DrawIconEx(hdc, S(22), S(20), g_icon, S(28), S(28), 0, NULL, DI_NORMAL);

    SelectObject(hdc, fTitle);
    SetTextColor(hdc, cText);
    RECT rT = { S(60), S(18), w - S(46), S(42) };
    DrawTextW(hdc, g_done ? L"AutoDark installato" : L"Installa AutoDark", -1, &rT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, fSmall);
    SetTextColor(hdc, cMuted);
    RECT rSub = { S(60), S(42), w - S(22), S(62) };
    DrawTextW(hdc, L"Tema Chiaro/Scuro automatico \u2022 Ultra-leggero", -1, &rSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (g_hover == 1) FillRoundRect(hdc, &rClose, S(8), cBtnHover, CLR_INVALID);
    SelectObject(hdc, fTitle);
    SetTextColor(hdc, cMuted);
    DrawTextW(hdc, L"\u00D7", -1, &rClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (!g_done) {
        // Checkbox 1
        RECT rc1 = { rChk1.left, rChk1.top + S(6), rChk1.left + S(17), rChk1.top + S(23) };
        FillRoundRect(hdc, &rc1, S(4), g_chkStartup ? cAccent : cBg, g_chkStartup ? cAccent : cBorder);
        if (g_chkStartup) {
            SelectObject(hdc, fSmall); SetTextColor(hdc, RGB(255,255,255));
            DrawTextW(hdc, L"\u2713", -1, &rc1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        RECT rl1 = { rc1.right + S(10), rChk1.top, rChk1.right, rChk1.bottom };
        SelectObject(hdc, fBody); SetTextColor(hdc, g_hover == 2 ? cText : cMuted);
        DrawTextW(hdc, L"Avvia automaticamente con Windows (in systray)", -1, &rl1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Checkbox 2
        RECT rc2 = { rChk2.left, rChk2.top + S(6), rChk2.left + S(17), rChk2.top + S(23) };
        FillRoundRect(hdc, &rc2, S(4), g_chkLaunch ? cAccent : cBg, g_chkLaunch ? cAccent : cBorder);
        if (g_chkLaunch) {
            SelectObject(hdc, fSmall); SetTextColor(hdc, RGB(255,255,255));
            DrawTextW(hdc, L"\u2713", -1, &rc2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        RECT rl2 = { rc2.right + S(10), rChk2.top, rChk2.right, rChk2.bottom };
        SelectObject(hdc, fBody); SetTextColor(hdc, g_hover == 3 ? cText : cMuted);
        DrawTextW(hdc, L"Avvia AutoDark subito al termine", -1, &rl2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Cancel button
        FillRoundRect(hdc, &rCancel, S(8), g_hover == 5 ? cBtnHover : cBg, cBorder);
        SelectObject(hdc, fBody); SetTextColor(hdc, cText);
        DrawTextW(hdc, L"Annulla", -1, &rCancel, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        RECT rMsg = { S(22), S(100), w - S(22), S(160) };
        SelectObject(hdc, fBody); SetTextColor(hdc, cText);
        DrawTextW(hdc, L"Installazione completata!\nTrovi l'icona di AutoDark nella barra di sistema (systray).",
                  -1, &rMsg, DT_LEFT | DT_WORDBREAK);
    }

    // Primary button
    FillRoundRect(hdc, &rOk, S(8), g_hover == 4 ? cAccHov : cAccent, CLR_INVALID);
    SelectObject(hdc, fBold); SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, g_done ? L"Chiudi" : L"Installa", -1, &rOk, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    BitBlt(hdcWin, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
    DeleteObject(fTitle); DeleteObject(fBody); DeleteObject(fBold); DeleteObject(fSmall);
    SelectObject(hdc, oldBmp); DeleteObject(hBmp); DeleteDC(hdc);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK SetupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        int corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        g_icon = CreateAutoDarkIcon(S(32), !g_dark);
        return 0;
    }
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProcW(hwnd, msg, wParam, lParam);
        if (hit == HTCLIENT) {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            ScreenToClient(hwnd, &pt);
            if (pt.y < S(70) && HitTest(pt) == 0) return HTCAPTION;
        }
        return hit;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int h = HitTest(pt);
        if (h != g_hover) {
            g_hover = h;
            InvalidateRect(hwnd, NULL, FALSE);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_hover != 0) { g_hover = 0; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_LBUTTONUP: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int h = HitTest(pt);
        if (h == 1 || h == 5) {
            DestroyWindow(hwnd);
        } else if (h == 2 && !g_done) {
            g_chkStartup = !g_chkStartup;
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (h == 3 && !g_done) {
            g_chkLaunch = !g_chkLaunch;
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (h == 4) {
            if (g_done) {
                DestroyWindow(hwnd);
            } else {
                if (PerformInstall()) {
                    g_done = TRUE;
                    InvalidateRect(hwnd, NULL, FALSE);
                } else {
                    MessageBoxW(hwnd, L"Impossibile completare l'installazione.", L"AutoDark", MB_ICONERROR);
                }
            }
        }
        return 0;
    }
    case WM_PAINT:
        PaintSetup(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_icon) DestroyIcon(g_icon);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmdLine, int nShow) {
    (void)hPrev; (void)nShow;

    if (lpCmdLine && wcsstr(lpCmdLine, L"--uninstall") != NULL) {
        PerformUninstall();
        MessageBoxW(NULL, L"AutoDark \u00E8 stato disinstallato.", L"AutoDark", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HDC hdcScreen = GetDC(NULL);
    g_dpi = (UINT)GetDeviceCaps(hdcScreen, LOGPIXELSX);
    ReleaseDC(NULL, hdcScreen);
    g_dark = IsWinDark();

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = SetupProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = INST_WND_CLASS;
    RegisterClassExW(&wc);

    int w = S(WIN_W), h = S(WIN_H);
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(0, INST_WND_CLASS, L"Installa AutoDark", WS_POPUP | WS_VISIBLE,
                                x, y, w, h, NULL, NULL, hInst, NULL);
    (void)hwnd;

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
