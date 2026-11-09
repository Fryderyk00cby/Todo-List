/* Inspect real Win32 control sizes using GDI at common display scales. */
#define WinMain unused_app_entry
#include "../src/main.c"
#undef WinMain
#include <assert.h>

static BOOL CALLBACK check_label(HWND control, LPARAM data) {
    (void)data;
    wchar_t cls[32], text[512];
    GetClassNameW(control, cls, 32);
    if (wcscmp(cls, L"Static") != 0) return TRUE;
    GetWindowTextW(control, text, 512);
    RECT bounds;
    GetClientRect(control, &bounds);
    HDC dc = GetDC(control);
    HFONT font = (HFONT)SendMessageW(control, WM_GETFONT, 0, 0);
    HGDIOBJ old = SelectObject(dc, font);
    RECT required = {0, 0, bounds.right, 0};
    DrawTextW(dc, text, -1, &required, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    assert(required.bottom <= bounds.bottom);
    assert(required.right <= bounds.right);
    SelectObject(dc, old);
    ReleaseDC(control, dc);
    return TRUE;
}

int main(void) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_DATE_CLASSES};
    InitCommonControlsEx(&icc);
    HWND owner = CreateWindowExW(0, L"STATIC", L"Layout test", WS_POPUP,
        0, 0, 1200, 1000, NULL, NULL, GetModuleHandleW(NULL), NULL);
    assert(owner);
    for (int dpi = 96; dpi <= 192; dpi += 48) {
        g_dpi = dpi;
        create_fonts();
        for (int lang = 0; lang < 2; ++lang) {
            app_language = lang;
            struct { DLGTEMPLATE dialog; WORD menu, cls, title; } resource;
            ZeroMemory(&resource, sizeof(resource));
            resource.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
            HWND dialog = CreateDialogIndirectParamW(GetModuleHandleW(NULL),
                &resource.dialog, owner, manual_focus_proc, 0);
            assert(dialog);
            EnumChildWindows(dialog, check_label, 0);
            HWND start = GetDlgItem(dialog, IDC_MANUAL_START);
            assert(SendMessageW(start, DTM_SETFORMATW, 0, (LPARAM)L"yyyy-MM-dd HH:mm"));
            wchar_t text[64];
            GetWindowTextW(start, text, 64);
            assert(wcslen(text) == 16 && text[4] == L'-' && text[13] == L':');
            HDC dc = GetDC(start);
            HGDIOBJ old = SelectObject(dc, g_font_normal);
            SIZE size;
            GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size);
            RECT rect;
            GetClientRect(start, &rect);
            assert(size.cx + scale(30) < rect.right);
            SelectObject(dc, old);
            ReleaseDC(start, dc);
            DestroyWindow(dialog);
        }
        DeleteObject(g_font_small); DeleteObject(g_font_title); DeleteObject(g_font_normal);
        DeleteObject(g_font_icon); DeleteObject(g_font_strike); DeleteObject(g_font_timer);
    }
    DestroyWindow(owner);
    puts("PASS: English/Chinese text fits at 100%, 150%, 200%; full date/time format and field width");
    return 0;
}
