#include "appearance.h"
#include <commctrl.h>
#include <wchar.h>

/* Compatibility with older MinGW Windows headers. */
#ifndef BS_TYPEMASK
#define BS_TYPEMASK 0x0000000FL
#endif
#ifndef ODS_NOFOCUSRECT
#define ODS_NOFOCUSRECT 0x0200
#endif

const AppTheme app_themes[3] = {
    {L"Sage", RGB(245,247,242), RGB(255,255,252), RGB(49,100,81),
        RGB(227,237,222), RGB(38,54,45), RGB(101,115,104), RGB(218,226,215)},
    {L"Lavender", RGB(247,245,250), RGB(255,254,255), RGB(110,82,150),
        RGB(237,229,246), RGB(53,43,67), RGB(115,102,128), RGB(228,219,237)},
    {L"Apricot", RGB(251,246,240), RGB(255,253,249), RGB(165,83,49),
        RGB(248,231,212), RGB(69,47,35), RGB(131,109,91), RGB(236,222,204)}
};
int app_theme_index;
static wchar_t settings_path[MAX_PATH];

void appearance_load(void) {
    DWORD len = GetModuleFileNameW(NULL, settings_path, MAX_PATH);
    if (!len || len >= MAX_PATH) { settings_path[0] = 0; return; }
    wchar_t *slash = wcsrchr(settings_path, L'\\');
    if (!slash || slash - settings_path + 16 >= MAX_PATH) {
        settings_path[0] = 0;
        return;
    }
    wcscpy(slash + 1, L"appearance.ini");
    int value = (int)GetPrivateProfileIntW(L"Appearance", L"Theme", 0, settings_path);
    app_theme_index = value >= 0 && value < 3 ? value : 0;
}

BOOL appearance_save(void) {
    wchar_t value[2] = {(wchar_t)(L'0' + app_theme_index), 0};
    return settings_path[0] && WritePrivateProfileStringW(L"Appearance", L"Theme", value, settings_path);
}

void appearance_round_rect(HDC dc, RECT rc, COLORREF fill, COLORREF border, int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_brush = SelectObject(dc, brush), old_pen = SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static LRESULT CALLBACK button_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
        UINT_PTR id, DWORD_PTR data) {
    (void)data;
    if (msg == WM_MOUSEMOVE && !GetPropW(hwnd, L"AppearanceHover")) {
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd, 0};
        SetPropW(hwnd, L"AppearanceHover", (HANDLE)1);
        TrackMouseEvent(&track);
        InvalidateRect(hwnd, NULL, FALSE);
    } else if (msg == WM_MOUSELEAVE) {
        RemovePropW(hwnd, L"AppearanceHover");
        InvalidateRect(hwnd, NULL, FALSE);
    } else if (msg == WM_NCDESTROY) {
        RemovePropW(hwnd, L"AppearanceHover");
        RemoveWindowSubclass(hwnd, button_proc, id);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void appearance_button_setup(HWND button) {
    LONG_PTR style = GetWindowLongPtrW(button, GWL_STYLE);
    SetWindowLongPtrW(button, GWL_STYLE, (style & ~BS_TYPEMASK) | BS_OWNERDRAW | WS_TABSTOP);
    SendMessageW(button, BM_SETSTYLE, BS_OWNERDRAW, TRUE);
    SetWindowSubclass(button, button_proc, 1, 0);
}

void appearance_draw_button(const DRAWITEMSTRUCT *item, HFONT font, int dpi, BOOL active, int swatch) {
    HDC dc = item->hDC;
    int saved = SaveDC(dc);
    RECT rc = item->rcItem;
    HBRUSH bg = CreateSolidBrush(THEME.background);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    BOOL disabled = (item->itemState & ODS_DISABLED) != 0;
    BOOL hover = GetPropW(item->hwndItem, L"AppearanceHover") != NULL;
    COLORREF fill = active ? THEME.accent : (hover ? THEME.tint : THEME.surface);
    COLORREF ink = active ? RGB(255,255,255) : THEME.ink;
    if (item->itemState & ODS_SELECTED) { fill = THEME.ink; ink = RGB(255,255,255); }
    if (disabled) { fill = THEME.background; ink = THEME.muted; }
    InflateRect(&rc, -1, -1);
    appearance_round_rect(dc, rc, fill, active ? fill : THEME.border, MulDiv(14, dpi, 96));
    if (swatch >= 0) {
        int dot = MulDiv(9, dpi, 96);
        RECT circle = {rc.left + dot, (rc.bottom + rc.top - dot) / 2,
            rc.left + dot * 2, (rc.bottom + rc.top + dot) / 2};
        appearance_round_rect(dc, circle, app_themes[swatch].accent,
            active ? RGB(255,255,255) : app_themes[swatch].accent, dot);
        rc.left += dot * 2;
    }
    wchar_t label[128];
    GetWindowTextW(item->hwndItem, label, 128);
    if (swatch == -2) wcscpy(label, L"\x2699"); /* Accessible name remains Settings. */
    SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, ink);
    DrawTextW(dc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if ((item->itemState & ODS_FOCUS) && !(item->itemState & ODS_NOFOCUSRECT)) {
        rc = item->rcItem;
        InflateRect(&rc, -MulDiv(5,dpi,96), -MulDiv(5,dpi,96));
        DrawFocusRect(dc, &rc);
    }
    RestoreDC(dc, saved);
}

static LRESULT CALLBACK header_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
        UINT_PTR id, DWORD_PTR data) {
    (void)data;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        int saved = SaveDC(dc);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH brush = CreateSolidBrush(THEME.surface);
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        SelectObject(dc, (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, THEME.muted);
        for (int i = 0; i < Header_GetItemCount(hwnd); ++i) {
            wchar_t label[128];
            HDITEMW item = {0};
            item.mask = HDI_TEXT;
            item.pszText = label;
            item.cchTextMax = 128;
            SendMessageW(hwnd, HDM_GETITEMW, i, (LPARAM)&item);
            Header_GetItemRect(hwnd, i, &rc);
            rc.left += 10;
            DrawTextW(dc, label, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        RestoreDC(dc, saved);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, header_proc, id);
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void appearance_header_setup(HWND list) {
    SetWindowSubclass(ListView_GetHeader(list), header_proc, 1, 0);
}
