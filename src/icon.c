#include "icon.h"
#include <math.h>
#include <stdlib.h>

HICON CreateAutoDarkIcon(int size, BOOL isLightActive) {
    BITMAPV5HEADER bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size        = sizeof(BITMAPV5HEADER);
    bi.bV5Width       = size;
    bi.bV5Height      = -size; // top-down
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    void *bits = NULL;
    HDC hdc = GetDC(NULL);
    HBITMAP hColor = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HBITMAP hMask  = CreateBitmap(size, size, 1, 1, NULL);
    ReleaseDC(NULL, hdc);

    if (!hColor || !bits) {
        if (hColor) DeleteObject(hColor);
        if (hMask)  DeleteObject(hMask);
        return NULL;
    }

    DWORD *px = (DWORD *)bits;
    double cx = size * 0.5;
    double cy = size * 0.5;
    double R  = size * 0.44;
    double rIn = R * 0.78;
    double border = R * 0.10;
    if (border < 1.0) border = 1.0;

    // Colors (R, G, B)
    // Warm amber sun, deep slate/indigo moon, subtle ring
    const double sunR = 255.0, sunG = 196.0, sunB = 54.0;
    const double moonR = isLightActive ? 45.0 : 120.0;
    const double moonG = isLightActive ? 52.0 : 148.0;
    const double moonB = isLightActive ? 78.0 : 235.0;
    const double ringR = isLightActive ? 235.0 : 210.0;
    const double ringG = isLightActive ? 235.0 : 220.0;
    const double ringB = isLightActive ? 240.0 : 240.0;

    const int SS = 4; // 4x4 supersampling
    const double invSS = 1.0 / (double)(SS * SS);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            double accR = 0, accG = 0, accB = 0, accA = 0;

            for (int sy = 0; sy < SS; sy++) {
                for (int sx = 0; sx < SS; sx++) {
                    double pxX = x + (sx + 0.5) / SS;
                    double pxY = y + (sy + 0.5) / SS;
                    double dx = pxX - cx;
                    double dy = pxY - cy;
                    double dist = sqrt(dx * dx + dy * dy);

                    if (dist <= R) {
                        double r, g, b, a = 1.0;
                        if (dist > rIn) {
                            // Outer crisp border ring
                            r = ringR; g = ringG; b = ringB;
                        } else {
                            // Diagonal split (sun top-left, moon bottom-right)
                            if (dx + dy < 0.0) {
                                r = sunR; g = sunG; b = sunB;
                            } else {
                                // Crescent highlight inside moon half
                                double cdx = dx - R * 0.22;
                                double cdy = dy - R * 0.16;
                                double cdist = sqrt(cdx * cdx + cdy * cdy);
                                if (cdist < R * 0.36 && (dx * 0.8 + dy * 0.8) > R * 0.15) {
                                    r = 225.0; g = 232.0; b = 255.0;
                                } else {
                                    r = moonR; g = moonG; b = moonB;
                                }
                            }
                        }
                        accR += r * a;
                        accG += g * a;
                        accB += b * a;
                        accA += a;
                    }
                }
            }

            double avgA = accA * invSS;
            BYTE outA = (BYTE)(avgA * 255.0 + 0.5);
            BYTE outR = 0, outG = 0, outB = 0;
            if (accA > 0.0) {
                // Premultiplied alpha for 32-bit DIB icon
                outR = (BYTE)((accR * invSS) + 0.5);
                outG = (BYTE)((accG * invSS) + 0.5);
                outB = (BYTE)((accB * invSS) + 0.5);
            }
            px[y * size + x] = ((DWORD)outA << 24) | ((DWORD)outR << 16) | ((DWORD)outG << 8) | (DWORD)outB;
        }
    }

    ICONINFO ii;
    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon    = TRUE;
    ii.hbmMask  = hMask;
    ii.hbmColor = hColor;

    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hColor);
    DeleteObject(hMask);
    return hIcon;
}
