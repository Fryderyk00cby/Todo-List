#ifndef MEMO_H
#define MEMO_H
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif
#include <windows.h>
#include <stdbool.h>

#define MEMO_TEXT_MAX 4096
typedef struct { char date[11]; wchar_t text[MEMO_TEXT_MAX]; } MemoEntry;
typedef struct { MemoEntry *items; int count, capacity; } MemoBook;
void memo_book_free(MemoBook *book);
const wchar_t *memo_book_get(const MemoBook *book, const char *date);
bool memo_book_set(MemoBook *book, const char *date, const wchar_t *text);
bool memo_book_load(MemoBook *book, const wchar_t *path);
bool memo_book_save(const MemoBook *book, const wchar_t *path);
HWND memo_create(HWND parent, int id);
void memo_style(HWND hwnd, int dpi, HFONT normal, HFONT small_font);
bool memo_flush(HWND hwnd);
void memo_reset(HWND hwnd);
#endif
