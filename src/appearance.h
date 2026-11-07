#ifndef APPEARANCE_H
#define APPEARANCE_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif
#include <windows.h>

typedef struct {
    const wchar_t *name;
    COLORREF background, surface, accent, tint, ink, muted, border;
} AppTheme;

extern const AppTheme app_themes[3];
extern int app_theme_index;
#define THEME (app_themes[app_theme_index])

void appearance_load(void);
BOOL appearance_save(void);
void appearance_round_rect(HDC dc, RECT rc, COLORREF fill, COLORREF border, int radius);
void appearance_button_setup(HWND button);
void appearance_draw_button(const DRAWITEMSTRUCT *item, HFONT font, int dpi, BOOL active, int swatch);
void appearance_header_setup(HWND list);

#endif
