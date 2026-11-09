#include "memo.h"
#include "appearance.h"
#include "language.h"
#include "study.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef WC_ERR_INVALID_CHARS
#define WC_ERR_INVALID_CHARS 0x80
#endif
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

void memo_book_free(MemoBook *book) {
    free(book->items);
    memset(book, 0, sizeof(*book));
}

const wchar_t *memo_book_get(const MemoBook *book, const char *date) {
    for (int i = 0; i < book->count; ++i)
        if (!strcmp(book->items[i].date, date)) return book->items[i].text;
    return L"";
}

bool memo_book_set(MemoBook *book, const char *date, const wchar_t *text) {
    char checked[11];
    if (!date || !text || strlen(date) != 10 || wcslen(text) >= MEMO_TEXT_MAX ||
        !study_date_add_days(date, 0, checked, sizeof(checked)) || strcmp(checked, date)) return false;
    int index = 0;
    while (index < book->count && strcmp(book->items[index].date, date)) ++index;
    if (!*text) {
        if (index < book->count) {
            memmove(book->items + index, book->items + index + 1,
                (size_t)(book->count - index - 1) * sizeof(MemoEntry));
            --book->count;
        }
        return true;
    }
    if (index == book->count) {
        if (book->count == book->capacity) {
            int capacity = book->capacity ? book->capacity * 2 : 16;
            MemoEntry *items = (MemoEntry *)realloc(book->items, (size_t)capacity * sizeof(MemoEntry));
            if (!items) return false;
            book->items = items; book->capacity = capacity;
        }
        ++book->count;
    }
    strcpy(book->items[index].date, date);
    wcscpy(book->items[index].text, text);
    return true;
}

/* UTF-8 lines: YYYY-MM-DD|text; escape backslashes and line breaks. */
bool memo_book_save(const MemoBook *book, const wchar_t *path) {
    wchar_t temporary[MAX_PATH + 8];
    if (wcslen(path) >= MAX_PATH) return false;
    wcscpy(temporary, path); wcscat(temporary, L".tmp");
    FILE *file = _wfopen(temporary, L"wb");
    if (!file) return false;
    bool ok = true;
    for (int i = 0; i < book->count && ok; ++i) {
        char utf8[MEMO_TEXT_MAX * 4];
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, book->items[i].text, -1,
                utf8, sizeof(utf8), NULL, NULL)) { ok = false; break; }
        if (fprintf(file, "%s|", book->items[i].date) < 0) { ok = false; break; }
        for (const char *p = utf8; *p; ++p) {
            if (*p == '\n' || *p == '\r' || *p == '\\') {
                if (fputc('\\', file) == EOF) ok = false;
                if (fputc(*p == '\n' ? 'n' : *p == '\r' ? 'r' : '\\', file) == EOF) ok = false;
            } else if (fputc((unsigned char)*p, file) == EOF) ok = false;
        }
        if (fputc('\n', file) == EOF) ok = false;
    }
    if (fclose(file)) ok = false;
    if (!ok || !MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary); return false;
    }
    return true;
}

bool memo_book_load(MemoBook *book, const wchar_t *path) {
    FILE *file = _wfopen(path, L"rb");
    if (!file) return GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES &&
        GetLastError() == ERROR_FILE_NOT_FOUND;
    MemoBook loaded = {0};
    char line[MEMO_TEXT_MAX * 8 + 32], utf8[MEMO_TEXT_MAX * 4];
    wchar_t text[MEMO_TEXT_MAX];
    bool ok = true;
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        if (length < 12 || line[10] != '|' || (line[length - 1] != '\n' && !feof(file))) { ok = false; break; }
        line[10] = 0;
        size_t n = 0;
        for (size_t j = 11; j < length && line[j] != '\n' && line[j] != '\r'; ++j) {
            char c = line[j];
            if (c == '\\') {
                c = line[++j];
                if (c == 'n') c = '\n';
                else if (c == 'r') c = '\r';
                else if (c != '\\') { ok = false; break; }
            }
            if (n + 1 >= sizeof(utf8)) { ok = false; break; }
            utf8[n++] = c;
        }
        utf8[n] = 0;
        if (!ok || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, text, MEMO_TEXT_MAX) ||
            !memo_book_set(&loaded, line, text)) { ok = false; break; }
    }
    if (ferror(file)) ok = false;
    fclose(file);
    if (!ok) { memo_book_free(&loaded); return false; }
    memo_book_free(book); *book = loaded;
    return true;
}

typedef struct {
    MemoBook book;
    HWND edit, prev, next, today, clear;
    HFONT normal, small_font;
    HBRUSH surface;
    int dpi, year, month;
    SYSTEMTIME selected;
    wchar_t path[MAX_PATH];
    bool loading, dirty, failed, load_failed;
    RECT calendar, paper, cells[42];
    int days[42];
} MemoView;

static int px(MemoView *v, int n) { return MulDiv(n, v->dpi, 96); }
static MemoView *view(HWND hwnd) { return (MemoView *)GetWindowLongPtrW(hwnd, GWLP_USERDATA); }
static void key_for(const SYSTEMTIME *st, char date[11]) { study_systemtime_to_date(st, date, 11); }

static LRESULT CALLBACK memo_edit_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
        UINT_PTR id, DWORD_PTR data) {
    MemoView *v = (MemoView *)data;
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, memo_edit_proc, id);
    LRESULT result = DefSubclassProc(hwnd, msg, wp, lp);
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) InvalidateRect(hwnd, NULL, TRUE);
    if (msg == WM_PAINT && !GetWindowTextLengthW(hwnd) && GetFocus() != hwnd && !v->load_failed) {
        HDC dc = GetDC(hwnd);
        int saved = SaveDC(dc);
        RECT rc; GetClientRect(hwnd, &rc); InflateRect(&rc, -2, -2);
        SelectObject(dc, v->normal); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, THEME.muted);
        DrawTextW(dc, TR(MEMO_PLACEHOLDER), -1, &rc, DT_WORDBREAK | DT_NOPREFIX);
        RestoreDC(dc, saved); ReleaseDC(hwnd, dc);
    }
    return result;
}

bool memo_flush(HWND hwnd) {
    MemoView *v = view(hwnd);
    if (!v || !v->dirty) return true;
    if (v->load_failed) return false;
    wchar_t text[MEMO_TEXT_MAX]; char date[11];
    GetWindowTextW(v->edit, text, MEMO_TEXT_MAX);
    key_for(&v->selected, date);
    v->failed = !memo_book_set(&v->book, date, text) || !memo_book_save(&v->book, v->path);
    if (!v->failed) v->dirty = false;
    InvalidateRect(hwnd, NULL, FALSE);
    return !v->failed;
}

static bool select_date(HWND hwnd, SYSTEMTIME selected) {
    MemoView *v = view(hwnd);
    if (!memo_flush(hwnd)) {
        MessageBoxW(hwnd, TR(MEMO_SAVE_ERROR), TR(MEMO_TAB), MB_OK | MB_ICONWARNING);
        return false;
    }
    v->selected = selected; v->year = selected.wYear; v->month = selected.wMonth;
    char date[11]; key_for(&selected, date);
    v->loading = true;
    SetWindowTextW(v->edit, memo_book_get(&v->book, date));
    v->loading = false;
    EnableWindow(v->clear, !v->load_failed && GetWindowTextLengthW(v->edit) > 0);
    InvalidateRect(hwnd, NULL, FALSE);
    return true;
}

static void memo_layout(HWND hwnd) {
    MemoView *v = view(hwnd); if (!v) return;
    RECT rc; GetClientRect(hwnd, &rc);
    int gap = px(v, 16), left = (rc.right - gap) * 53 / 100;
    v->calendar = (RECT){0, 0, left, rc.bottom};
    v->paper = (RECT){left + gap, 0, rc.right, rc.bottom};
    SetWindowPos(v->prev, NULL, px(v, 14), px(v, 16), px(v, 34), px(v, 34), SWP_NOZORDER);
    SetWindowPos(v->next, NULL, left - px(v, 48), px(v, 16), px(v, 34), px(v, 34), SWP_NOZORDER);
    SetWindowPos(v->today, NULL, left / 2 - px(v, 44), rc.bottom - px(v, 46), px(v, 88), px(v, 32), SWP_NOZORDER);
    SetWindowPos(v->clear, NULL, rc.right - px(v, 90), px(v, 16), px(v, 72), px(v, 32), SWP_NOZORDER);
    SetWindowPos(v->edit, NULL, v->paper.left + px(v, 18), px(v, 92),
        v->paper.right - v->paper.left - px(v, 36), rc.bottom - px(v, 168), SWP_NOZORDER);
}

static void label(HDC dc, HFONT font, COLORREF color, const wchar_t *text, RECT rc, UINT flags) {
    SelectObject(dc, font); SetTextColor(dc, color); SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text, -1, &rc, flags | DT_NOPREFIX);
}

static void memo_paint(HWND hwnd, HDC dc) {
    MemoView *v = view(hwnd);
    RECT rc; GetClientRect(hwnd, &rc);
    HBRUSH bg = CreateSolidBrush(THEME.background); FillRect(dc, &rc, bg); DeleteObject(bg);
    appearance_round_rect(dc, v->calendar, THEME.surface, THEME.border, px(v, 20));
    appearance_round_rect(dc, v->paper, THEME.surface, THEME.border, px(v, 20));
    wchar_t title[64];
    _snwprintf(title, 64, L"%04d / %02d", v->year, v->month);
    RECT header = {px(v, 50), px(v, 16), v->calendar.right - px(v, 50), px(v, 50)};
    label(dc, v->normal, THEME.ink, title, header, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SYSTEMTIME first = {0}, normalized;
    first.wYear = (WORD)v->year; first.wMonth = (WORD)v->month; first.wDay = 1;
    FILETIME ft; SystemTimeToFileTime(&first, &ft); FileTimeToSystemTime(&ft, &normalized);
    int offset = (normalized.wDayOfWeek + 6) % 7;
    int days = 31;
    if (v->month == 4 || v->month == 6 || v->month == 9 || v->month == 11) days = 30;
    if (v->month == 2) days = (v->year % 4 == 0 && (v->year % 100 != 0 || v->year % 400 == 0)) ? 29 : 28;
    int grid_left = px(v, 12), grid_width = v->calendar.right - px(v, 24);
    int grid_top = px(v, 92), row_height = (rc.bottom - px(v, 150)) / 6;
    SYSTEMTIME now; GetLocalTime(&now);
    const TextId weekdays[] = {TEXT_MON, TEXT_TUE, TEXT_WED, TEXT_THU, TEXT_FRI, TEXT_SAT, TEXT_SUN};
    for (int col = 0; col < 7; ++col) {
        RECT r = {grid_left + grid_width * col / 7, px(v, 62), grid_left + grid_width * (col + 1) / 7, px(v, 86)};
        label(dc, v->small_font, THEME.muted, language_text(weekdays[col]), r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (int i = 0; i < 42; ++i) {
        RECT r = {grid_left + grid_width * (i % 7) / 7 + px(v, 2), grid_top + row_height * (i / 7),
            grid_left + grid_width * (i % 7 + 1) / 7 - px(v, 2), grid_top + row_height * (i / 7 + 1) - px(v, 3)};
        v->cells[i] = r; int day = i - offset + 1;
        v->days[i] = day > 0 && day <= days ? day : 0;
        if (!v->days[i]) continue;
        bool chosen = v->selected.wYear == v->year && v->selected.wMonth == v->month && v->selected.wDay == day;
        bool today = now.wYear == v->year && now.wMonth == v->month && now.wDay == day;
        if (chosen || today) appearance_round_rect(dc, r, chosen ? THEME.accent : THEME.tint,
            chosen ? THEME.accent : THEME.border, px(v, 12));
        if (chosen && GetFocus() == hwnd) {
            RECT focus = r; InflateRect(&focus, -px(v, 3), -px(v, 3)); DrawFocusRect(dc, &focus);
        }
        wchar_t number[8]; _snwprintf(number, 8, L"%d", day);
        RECT text = r; text.bottom -= px(v, 5);
        label(dc, v->normal, chosen ? RGB(255,255,255) : THEME.ink, number, text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        char date[11]; snprintf(date, sizeof(date), "%04d-%02d-%02d", v->year, v->month, day);
        if (*memo_book_get(&v->book, date)) {
            int x = (r.left + r.right) / 2, y = r.bottom - px(v, 6);
            RECT dot = {x - px(v, 2), y - px(v, 2), x + px(v, 2), y + px(v, 2)};
            appearance_round_rect(dc, dot, chosen ? RGB(255,255,255) : THEME.accent,
                chosen ? RGB(255,255,255) : THEME.accent, px(v, 4));
        }
    }
    _snwprintf(title, 64, L"%04d / %02d / %02d", v->selected.wYear, v->selected.wMonth, v->selected.wDay);
    RECT date = {v->paper.left + px(v, 18), px(v, 18), rc.right - px(v, 102), px(v, 46)};
    label(dc, v->normal, THEME.ink, title, date, DT_SINGLELINE | DT_VCENTER);
    date.top = px(v, 54); date.bottom = px(v, 82); date.right = rc.right - px(v, 18);
    label(dc, v->small_font, THEME.muted, TR(MEMO_DAY_HINT), date, DT_WORDBREAK);
    RECT status = {v->paper.left + px(v, 18), rc.bottom - px(v, 60), rc.right - px(v, 18), rc.bottom - px(v, 12)};
    label(dc, v->small_font, v->failed || v->load_failed ? RGB(180,64,48) : THEME.muted,
        v->load_failed ? TR(MEMO_LOAD_ERROR) : v->failed ? TR(MEMO_SAVE_ERROR) : v->dirty ? TR(MEMO_SAVING) : TR(MEMO_SAVED),
        status, DT_WORDBREAK);
}

void memo_style(HWND hwnd, int dpi, HFONT normal, HFONT small_font) {
    MemoView *v = view(hwnd); if (!v) return;
    v->dpi = dpi; v->normal = normal; v->small_font = small_font;
    if (v->surface) DeleteObject(v->surface);
    v->surface = CreateSolidBrush(THEME.surface);
    HWND controls[] = {v->edit, v->prev, v->next, v->today, v->clear};
    for (int i = 0; i < 5; ++i) SendMessageW(controls[i], WM_SETFONT, (WPARAM)normal, TRUE);
    SetWindowTextW(v->today, TR(MEMO_TODAY));
    SetWindowTextW(v->clear, TR(MEMO_CLEAR));
    SendMessageW(v->edit, EM_SETCUEBANNER, TRUE, (LPARAM)TR(MEMO_PLACEHOLDER));
    memo_layout(hwnd);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void memo_reset(HWND hwnd) {
    MemoView *v = view(hwnd); if (!v) return;
    KillTimer(hwnd, 1); memo_book_free(&v->book);
    v->dirty = v->failed = v->load_failed = false;
    GetLocalTime(&v->selected); v->year = v->selected.wYear; v->month = v->selected.wMonth;
    v->loading = true; SetWindowTextW(v->edit, L""); v->loading = false;
    EnableWindow(v->edit, TRUE); InvalidateRect(hwnd, NULL, FALSE);
    EnableWindow(v->clear, FALSE);
}

static LRESULT CALLBACK memo_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MemoView *v = view(hwnd);
    if (msg == WM_CREATE) {
        v = (MemoView *)calloc(1, sizeof(*v)); if (!v) return -1;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)v); v->dpi = 96;
        DWORD len = GetModuleFileNameW(NULL, v->path, MAX_PATH);
        wchar_t *slash = wcsrchr(v->path, L'\\');
        if (!len || len >= MAX_PATH || !slash || slash - v->path + 10 >= MAX_PATH) return -1;
        wcscpy(slash + 1, L"memos.dat");
        v->load_failed = !memo_book_load(&v->book, v->path);
        v->edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
            0,0,0,0,hwnd,(HMENU)1,NULL,NULL);
        SendMessageW(v->edit, EM_SETLIMITTEXT, MEMO_TEXT_MAX - 1, 0);
        SetWindowSubclass(v->edit, memo_edit_proc, 1, (DWORD_PTR)v);
        EnableWindow(v->edit, !v->load_failed);
        v->prev = CreateWindowExW(0,L"BUTTON",L"<",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,hwnd,(HMENU)2,NULL,NULL);
        v->next = CreateWindowExW(0,L"BUTTON",L">",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,hwnd,(HMENU)3,NULL,NULL);
        v->today = CreateWindowExW(0,L"BUTTON",TR(MEMO_TODAY),WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,hwnd,(HMENU)4,NULL,NULL);
        v->clear = CreateWindowExW(0,L"BUTTON",TR(MEMO_CLEAR),WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,hwnd,(HMENU)5,NULL,NULL);
        appearance_button_setup(v->prev); appearance_button_setup(v->next); appearance_button_setup(v->today);
        appearance_button_setup(v->clear);
        SYSTEMTIME now; GetLocalTime(&now); select_date(hwnd, now);
        return 0;
    }
    if (!v) return DefWindowProcW(hwnd,msg,wp,lp);
    switch (msg) {
    case WM_SETFOCUS: case WM_KILLFOCUS: InvalidateRect(hwnd,NULL,FALSE); return 0;
    case WM_SIZE: memo_layout(hwnd); InvalidateRect(hwnd,NULL,FALSE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bitmap = CreateCompatibleBitmap(dc,rc.right,rc.bottom);
        HGDIOBJ old = SelectObject(mem,bitmap); memo_paint(hwnd,mem);
        BitBlt(dc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);
        SelectObject(mem,old); DeleteObject(bitmap); DeleteDC(mem); EndPaint(hwnd,&ps); return 0;
    }
    case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wp,THEME.surface); SetTextColor((HDC)wp,THEME.ink); return (LRESULT)v->surface;
    case WM_DRAWITEM: appearance_draw_button((DRAWITEMSTRUCT *)lp,v->normal,v->dpi,FALSE,FALSE); return TRUE;
    case WM_TIMER: KillTimer(hwnd,1); memo_flush(hwnd); return 0;
    case WM_COMMAND:
        if (LOWORD(wp)==1 && HIWORD(wp)==EN_CHANGE && !v->loading) {
            v->dirty = true; SetTimer(hwnd,1,600,NULL); InvalidateRect(hwnd,NULL,FALSE);
            EnableWindow(v->clear, !v->load_failed && GetWindowTextLengthW(v->edit) > 0);
        } else if (LOWORD(wp)==5 && HIWORD(wp)==BN_CLICKED && !v->load_failed && GetWindowTextLengthW(v->edit)) {
            wchar_t previous[MEMO_TEXT_MAX]; char date[11];
            GetWindowTextW(v->edit,previous,MEMO_TEXT_MAX);
            key_for(&v->selected,date);
            KillTimer(hwnd,1);
            v->loading=true; SetWindowTextW(v->edit,L""); v->loading=false;
            v->dirty=true;
            if (!memo_flush(hwnd)) {
                /* Keep the note available when deleting its saved record fails. */
                memo_book_set(&v->book,date,previous);
                v->loading=true; SetWindowTextW(v->edit,previous); v->loading=false;
            }
            EnableWindow(v->clear,GetWindowTextLengthW(v->edit)>0);
            SetFocus(v->edit);
        } else if (LOWORD(wp)==4) {
            SYSTEMTIME now; GetLocalTime(&now); select_date(hwnd,now);
        } else if (LOWORD(wp)==2 || LOWORD(wp)==3) {
            int month = v->month + (LOWORD(wp)==2 ? -1 : 1), year = v->year;
            if (month==0) { month=12; --year; } if (month==13) { month=1; ++year; }
            if (year >= 1601 && year <= 9999) {
                v->year=year; v->month=month; InvalidateRect(hwnd,NULL,FALSE);
            }
        }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT p = {(short)LOWORD(lp),(short)HIWORD(lp)};
        for (int i=0;i<42;++i) if (v->days[i] && PtInRect(&v->cells[i],p)) {
            SYSTEMTIME st = {0}; st.wYear=(WORD)v->year; st.wMonth=(WORD)v->month; st.wDay=(WORD)v->days[i];
            select_date(hwnd,st); SetFocus(hwnd); break;
        }
        return 0;
    }
    case WM_KEYDOWN: {
        int delta = wp==VK_LEFT ? -1 : wp==VK_RIGHT ? 1 : wp==VK_UP ? -7 : wp==VK_DOWN ? 7 : 0;
        if (delta) {
            char date[11], shifted[11]; key_for(&v->selected,date);
            if (study_date_add_days(date,delta,shifted,sizeof(shifted))) {
                SYSTEMTIME st={0}; int y,m,d; sscanf(shifted,"%d-%d-%d",&y,&m,&d);
                st.wYear=(WORD)y;st.wMonth=(WORD)m;st.wDay=(WORD)d;select_date(hwnd,st);
            }
        }
        if (wp==VK_RETURN) SetFocus(v->edit);
        return 0;
    }
    case WM_NCDESTROY:
        KillTimer(hwnd,1); memo_book_free(&v->book); if(v->surface)DeleteObject(v->surface);
        free(v);SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);break;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

HWND memo_create(HWND parent, int id) {
    WNDCLASSW cls={0}; cls.lpfnWndProc=memo_proc;cls.hInstance=GetModuleHandleW(NULL);
    cls.lpszClassName=L"DaylightMemo";cls.hCursor=LoadCursorW(NULL,(LPCWSTR)IDC_ARROW);
    RegisterClassW(&cls);
    return CreateWindowExW(WS_EX_CONTROLPARENT,cls.lpszClassName,L"",WS_CHILD|WS_TABSTOP|WS_CLIPCHILDREN,
        0,0,0,0,parent,(HMENU)(INT_PTR)id,cls.hInstance,NULL);
}
