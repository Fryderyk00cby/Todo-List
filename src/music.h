#ifndef MUSIC_H
#define MUSIC_H
#include <windows.h>
HWND music_create(HWND parent, int id);
void music_style(HWND hwnd, int dpi, HFONT font);
void music_reset(HWND hwnd);
#endif
