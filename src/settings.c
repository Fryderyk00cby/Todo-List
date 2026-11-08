#include "settings.h"
#include "language.h"
#include <wchar.h>

static HWND theme_buttons[3];
static HWND language_buttons[LANGUAGE_COUNT];
static HFONT normal_font, small_font;

static int px(int value, int dpi) { return MulDiv(value, dpi, 96); }

static void label(HDC dc, const wchar_t *text, RECT rc, HFONT font, COLORREF color) {
    SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

void settings_create(HWND parent) {
    for (int i = 0; i < 3; ++i)
        theme_buttons[i] = CreateWindowExW(0, L"BUTTON", L"",
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, parent,
            (HMENU)(INT_PTR)(IDC_THEME_FIRST + i), NULL, NULL);
    const wchar_t *names[] = {L"English", L"简体中文"};
    for (int i = 0; i < LANGUAGE_COUNT; ++i)
        language_buttons[i] = CreateWindowExW(0, L"BUTTON", names[i],
            WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, parent,
            (HMENU)(INT_PTR)(IDC_LANGUAGE_FIRST + i), NULL, NULL);
    settings_refresh_language();
}

void settings_refresh_language(void) {
    const wchar_t *names[] = {TR(SAGE), TR(LAVENDER), TR(APRICOT)};
    for (int i = 0; i < 3; ++i) SetWindowTextW(theme_buttons[i], names[i]);
}

void settings_show(BOOL visible) {
    for (int i = 0; i < 3; ++i) ShowWindow(theme_buttons[i], visible ? SW_SHOW : SW_HIDE);
    for (int i = 0; i < LANGUAGE_COUNT; ++i)
        ShowWindow(language_buttons[i], visible ? SW_SHOW : SW_HIDE);
}

void settings_apply_fonts(HFONT normal, HFONT small_typeface) {
    normal_font = normal;
    small_font = small_typeface;
    for (int i = 0; i < 3; ++i)
        SendMessageW(theme_buttons[i], WM_SETFONT, (WPARAM)normal, TRUE);
    for (int i = 0; i < LANGUAGE_COUNT; ++i)
        SendMessageW(language_buttons[i], WM_SETFONT, (WPARAM)normal, TRUE);
}

void settings_focus(void) { SetFocus(theme_buttons[app_theme_index]); }

void settings_layout(HWND parent, int dpi) {
    RECT rc;
    GetClientRect(parent, &rc);
    int width = (rc.right - px(120, dpi)) / 3;
    for (int i = 0; i < 3; ++i)
        SetWindowPos(theme_buttons[i], NULL, px(48, dpi) + i * (width + px(12, dpi)),
            px(238, dpi), width, px(178, dpi), SWP_NOZORDER | SWP_NOACTIVATE);
    for (int i = 0; i < LANGUAGE_COUNT; ++i)
        SetWindowPos(language_buttons[i], NULL, px(48 + i * 190, dpi), px(578, dpi),
            px(178, dpi), px(42, dpi), SWP_NOZORDER | SWP_NOACTIVATE);
}

void settings_paint(HWND parent, HDC dc, int dpi) {
    RECT rc;
    GetClientRect(parent, &rc);
    RECT card = {px(24,dpi), px(150,dpi), rc.right - px(24,dpi), px(472,dpi)};
    appearance_round_rect(dc, card, THEME.surface, THEME.border, px(20,dpi));
    RECT text = {px(48,dpi), px(168,dpi), rc.right - px(48,dpi), px(196,dpi)};
    label(dc, TR(APPEARANCE), text, normal_font, THEME.ink);
    text.top = px(199,dpi); text.bottom = px(224,dpi);
    label(dc, TR(APPEARANCE_HINT), text, small_font, THEME.muted);
    text.top = px(432,dpi); text.bottom = px(456,dpi);
    label(dc, TR(INSTANT_HINT), text, small_font, THEME.muted);
    card.top = px(490,dpi); card.bottom = px(646,dpi);
    appearance_round_rect(dc, card, THEME.surface, THEME.border, px(20,dpi));
    text.top = px(508,dpi); text.bottom = px(536,dpi);
    label(dc, TR(LANGUAGE), text, normal_font, THEME.ink);
    text.top = px(540,dpi); text.bottom = px(565,dpi);
    label(dc, TR(LANGUAGE_HINT), text, small_font, THEME.muted);
}

BOOL settings_draw_item(const DRAWITEMSTRUCT *item, int dpi) {
    int language = (int)item->CtlID - IDC_LANGUAGE_FIRST;
    if (item->CtlType == ODT_BUTTON && language >= 0 && language < LANGUAGE_COUNT) {
        appearance_draw_button(item, normal_font, dpi, language == (int)app_language, FALSE);
        return TRUE;
    }
    int index = (int)item->CtlID - IDC_THEME_FIRST;
    if (item->CtlType != ODT_BUTTON || index < 0 || index >= 3) return FALSE;
    const AppTheme *theme = &app_themes[index];
    HDC dc = item->hDC;
    int saved = SaveDC(dc);
    RECT rc = item->rcItem;
    HBRUSH bg = CreateSolidBrush(THEME.surface);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    InflateRect(&rc, -1, -1);
    BOOL selected = index == app_theme_index;
    BOOL hover = GetPropW(item->hwndItem, L"AppearanceHover") != NULL;
    appearance_round_rect(dc, rc, theme->background,
        selected || hover ? theme->accent : THEME.border, px(16,dpi));
    RECT preview = {px(16,dpi), px(16,dpi), rc.right - px(16,dpi), px(94,dpi)};
    appearance_round_rect(dc, preview, theme->surface, theme->border, px(10,dpi));
    RECT line = {px(28,dpi), px(30,dpi), rc.right - px(28,dpi), px(42,dpi)};
    appearance_round_rect(dc, line, theme->tint, theme->tint, px(5,dpi));
    line.top = px(53,dpi); line.bottom = px(59,dpi); line.right -= px(24,dpi);
    appearance_round_rect(dc, line, theme->border, theme->border, px(3,dpi));
    line.top = px(69,dpi); line.bottom = px(78,dpi); line.right = px(75,dpi);
    appearance_round_rect(dc, line, theme->accent, theme->accent, px(4,dpi));
    RECT text = {px(16,dpi), px(106,dpi), rc.right - px(16,dpi), px(134,dpi)};
    wchar_t name[64];
    GetWindowTextW(item->hwndItem, name, 64);
    label(dc, name, text, normal_font, theme->ink);
    text.top = px(140,dpi); text.bottom = px(162,dpi);
    label(dc, selected ? TR(SELECTED) : TR(CLICK_APPLY), text, small_font, theme->accent);
    if (item->itemState & ODS_FOCUS) {
        RECT focus = rc;
        InflateRect(&focus, -px(5,dpi), -px(5,dpi));
        DrawFocusRect(dc, &focus);
    }
    RestoreDC(dc, saved);
    return TRUE;
}
