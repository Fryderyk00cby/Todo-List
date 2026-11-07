#ifndef SETTINGS_H
#define SETTINGS_H

#include "appearance.h"

#define IDC_THEME_FIRST 1200

/* Settings sections own their controls and drawing here; navigation stays in main.c. */
void settings_create(HWND parent);
void settings_show(BOOL visible);
void settings_layout(HWND parent, int dpi);
void settings_apply_fonts(HFONT normal, HFONT small_typeface);
void settings_paint(HWND parent, HDC dc, int dpi);
BOOL settings_draw_item(const DRAWITEMSTRUCT *item, int dpi);
void settings_focus(void);

#endif
