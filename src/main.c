#include "todo.h"
#include "study.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#ifndef CDDS_SUBITEMPREPAINT
#define CDDS_SUBITEMPREPAINT (CDDS_ITEMPREPAINT | 0x00020000)
#endif
#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifdef _MSC_VER
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#define IDC_INPUT           1001
#define IDC_ADD             1002
#define IDC_LIST            1003
#define IDC_DELETE          1004
#define IDC_DUE_CHECK       1005
#define IDC_DUE_DATE        1006
#define IDC_EDIT_BOX        1007

#define IDC_TAB_TODO        1101
#define IDC_TAB_STUDY       1102
#define IDC_STUDY_INPUT     1103
#define IDC_STUDY_TIMER     1104
#define IDC_STUDY_STATUS    1105
#define IDC_STUDY_START     1106
#define IDC_STUDY_HISTORY   1107
#define IDC_HIST_BACK       1108
#define IDC_HIST_PREV       1109
#define IDC_HIST_NEXT       1110
#define IDC_HIST_DATE       1111
#define IDC_HIST_TOTAL      1112
#define IDC_HIST_LIST       1113
#define IDC_HIST_DELETE     1114
#define IDC_HIST_WEEK       1115
#define IDC_WEEK_BACK       1116
#define IDC_WEEK_PREV       1117
#define IDC_WEEK_NEXT       1118
#define IDC_WEEK_LABEL      1119

#define IDM_ADD             2001
#define IDM_DELETE          2002

#define IDI_APPICON         101
#define TIMER_STUDY_ID      1

#define MARGIN              14
#define GAP                 8
#define HEADER_HEIGHT       50
#define TAB_HEIGHT          32
#define INPUT_HEIGHT        38
#define BUTTON_WIDTH        84
#define BUTTON_HEIGHT       38
#define INPUT_MAX_WIDTH     260
#define DUE_CHECK_WIDTH     58
#define DUE_DATE_WIDTH      130
#define GAP_DUE_TO_DATE     12
#define BOTTOM_HEIGHT       48
#define DUE_COL_WIDTH       120
#define WINDOW_WIDTH        600
#define WINDOW_HEIGHT       540

#define SWP_LAYOUT          (SWP_NOZORDER | SWP_NOCOPYBITS)

#define CLR_BG              RGB(245, 246, 248)
#define CLR_SURFACE         RGB(255, 255, 255)
#define CLR_ACCENT          RGB(0, 120, 212)
#define CLR_TITLE           RGB(32, 32, 32)
#define CLR_MUTED           RGB(110, 110, 110)
#define CLR_DONE            RGB(140, 140, 140)
#define CLR_OVERDUE         RGB(196, 43, 28)
#define CLR_SELECT_BG       RGB(232, 244, 253)
#define CLR_SELECT_FG       RGB(32, 32, 32)

typedef enum {
    VIEW_TODO = 0,
    VIEW_STUDY = 1,
    VIEW_HISTORY = 2,
    VIEW_WEEK = 3
} AppView;

static HWND g_hwnd_input;
static HWND g_hwnd_list;
static HWND g_hwnd_due_check;
static HWND g_hwnd_due_date;
static HWND g_hwnd_edit_box;
static HWND g_hwnd_tab_todo;
static HWND g_hwnd_tab_study;
static HWND g_hwnd_study_input;
static HWND g_hwnd_study_timer;
static HWND g_hwnd_study_status;
static HWND g_hwnd_study_start;
static HWND g_hwnd_study_history;
static HWND g_hwnd_hist_back;
static HWND g_hwnd_hist_prev;
static HWND g_hwnd_hist_next;
static HWND g_hwnd_hist_date;
static HWND g_hwnd_hist_total;
static HWND g_hwnd_hist_list;
static HWND g_hwnd_hist_delete;
static HWND g_hwnd_hist_week;
static HWND g_hwnd_week_back;
static HWND g_hwnd_week_prev;
static HWND g_hwnd_week_next;
static HWND g_hwnd_week_label;

static TodoList g_todos;
static StudyLog g_study_log;
static wchar_t g_data_path[MAX_PATH];
static wchar_t g_study_path[MAX_PATH];
static char g_history_date[STUDY_MAX_DATE];

static bool g_updating_list;
static bool g_programmatic_select;
static int g_selected_index;
static int g_editing_index = -1;
static int g_hist_selected_row = -1;
static bool g_hist_programmatic_select;
static int g_dpi = 96;
static int g_week_offset = 0;
static AppView g_view = VIEW_TODO;

static bool g_study_running;
static DWORD g_study_start_tick;
static SYSTEMTIME g_study_start_st;
static char g_study_task[STUDY_MAX_TEXT];
static wchar_t g_study_timer_last[32];

static HFONT g_font_title;
static HFONT g_font_normal;
static HFONT g_font_strike;
static HFONT g_font_timer;
static HBRUSH g_brush_bg;
static HBRUSH g_brush_surface;

static void layout_controls(HWND hwnd);
static void refresh_list(void);
static void add_task_from_input(HWND hwnd);
static void delete_selected_task(HWND hwnd);
static void update_due_date_enabled(void);
static void get_deadline_from_ui(char *buf, int bufsize);
static void end_inline_edit(bool commit);
static void switch_view(HWND hwnd, AppView view);
static void update_study_timer_display(void);
static void start_study(HWND hwnd);
static void stop_study(HWND hwnd);
static void refresh_history(void);
static void set_history_date_from_picker(void);
static void delete_selected_history(HWND hwnd);
static void update_study_clock_timer(HWND hwnd);
static void sync_hist_selection_visual(void);
static void paint_header_dc(HWND hwnd, HDC hdc);
static void paint_week_chart(HWND hwnd, HDC hdc);
static void update_week_label(void);
static void week_navigate(HWND hwnd, int delta);

static int scale(int value) {
    return MulDiv(value, g_dpi, 96);
}

static int get_window_dpi(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
        GetDpiForWindowFn get_dpi = (GetDpiForWindowFn)(void *)GetProcAddress(user32, "GetDpiForWindow");
        if (get_dpi) {
            UINT dpi = get_dpi(hwnd);
            if (dpi > 0) {
                return (int)dpi;
            }
        }
    }

    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi > 0 ? dpi : 96;
}

static void wide_to_utf8(const wchar_t *wide, char *out, int outsize) {
    int len = (int)wcslen(wide);
    int written = 0;

    while (len > 0) {
        written = WideCharToMultiByte(CP_UTF8, 0, wide, len, out, outsize - 1, NULL, NULL);
        if (written > 0) {
            break;
        }
        len--;
    }
    out[written > 0 ? written : 0] = '\0';
}

static void utf8_to_wide(const char *utf8, wchar_t *out, int outcount) {
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, outcount)) {
        out[0] = L'\0';
    }
}

static void create_fonts(void) {
    g_font_title = CreateFontW(
        -scale(24), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_font_normal = CreateFontW(
        -scale(18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_font_strike = CreateFontW(
        -scale(18), 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_font_timer = CreateFontW(
        -scale(56), 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    if (!g_font_timer) {
        g_font_timer = CreateFontW(
            -scale(56), 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Courrier New");
    }
}

static void destroy_fonts(void) {
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_normal) DeleteObject(g_font_normal);
    if (g_font_strike) DeleteObject(g_font_strike);
    if (g_font_timer) DeleteObject(g_font_timer);
    g_font_title = NULL;
    g_font_normal = NULL;
    g_font_strike = NULL;
    g_font_timer = NULL;
}

static void set_visible(HWND hwnd, bool visible) {
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

static void invalidate_child_controls(HWND parent) {
    for (HWND child = GetWindow(parent, GW_CHILD); child != NULL; child = GetWindow(child, GW_HWNDNEXT)) {
        if (child == g_hwnd_study_timer) {
            InvalidateRect(child, NULL, FALSE);
        } else {
            InvalidateRect(child, NULL, TRUE);
        }
    }
}

static void get_min_window_size(HWND hwnd, LONG *out_w, LONG *out_h) {
    RECT rc = {0, 0, scale(WINDOW_WIDTH), scale(WINDOW_HEIGHT)};
    AdjustWindowRectEx(&rc, (DWORD)GetWindowLongW(hwnd, GWL_STYLE),
            GetMenu(hwnd) != NULL, (DWORD)GetWindowLongW(hwnd, GWL_EXSTYLE));
    *out_w = rc.right - rc.left;
    *out_h = rc.bottom - rc.top;
}

static void apply_fonts(HWND hwnd) {
    SendMessageW(g_hwnd_input, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_due_check, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_due_date, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(GetDlgItem(hwnd, IDC_ADD), WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_list, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(GetDlgItem(hwnd, IDC_DELETE), WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_edit_box, WM_SETFONT, (WPARAM)g_font_normal, TRUE);

    SendMessageW(g_hwnd_tab_todo, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_tab_study, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_study_input, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_study_timer, WM_SETFONT, (WPARAM)g_font_timer, TRUE);
    SendMessageW(g_hwnd_study_status, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_study_start, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_study_history, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_hist_back, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_hist_prev, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_hist_next, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_hist_date, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_hist_total, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_hist_list, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_hist_delete, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_hist_week, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_week_back, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_week_prev, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_week_next, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    SendMessageW(g_hwnd_week_label, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
}

static void layout_todo_controls(int width, int height, int top) {
    int right = width - scale(MARGIN);
    int fixed_right = scale(BUTTON_WIDTH + GAP + DUE_DATE_WIDTH + GAP_DUE_TO_DATE + DUE_CHECK_WIDTH + GAP);
    int input_width = right - scale(MARGIN) - fixed_right;

    if (input_width > scale(INPUT_MAX_WIDTH)) {
        input_width = scale(INPUT_MAX_WIDTH);
    }
    if (input_width < scale(120)) {
        input_width = scale(120);
    }

    int list_top = top + scale(INPUT_HEIGHT + GAP);
    int list_height = height - list_top - scale(BOTTOM_HEIGHT + MARGIN);
    if (list_height < scale(80)) {
        list_height = scale(80);
    }

    int x = scale(MARGIN);
    SetWindowPos(g_hwnd_input, NULL, x, top, input_width, scale(INPUT_HEIGHT), SWP_LAYOUT);
    x += input_width + scale(GAP);
    SetWindowPos(g_hwnd_due_check, NULL, x, top + scale(8), scale(DUE_CHECK_WIDTH), scale(INPUT_HEIGHT), SWP_LAYOUT);
    x += scale(DUE_CHECK_WIDTH + GAP_DUE_TO_DATE);
    SetWindowPos(g_hwnd_due_date, NULL, x, top + scale(4), scale(DUE_DATE_WIDTH), scale(INPUT_HEIGHT), SWP_LAYOUT);
    x += scale(DUE_DATE_WIDTH + GAP);
    SetWindowPos(GetDlgItem(GetParent(g_hwnd_input), IDC_ADD), NULL, x, top, scale(BUTTON_WIDTH), scale(BUTTON_HEIGHT), SWP_LAYOUT);
    SetWindowPos(g_hwnd_list, NULL, scale(MARGIN), list_top, width - scale(MARGIN) * 2, list_height, SWP_LAYOUT);

    int delete_width = scale(130);
    SetWindowPos(
        GetDlgItem(GetParent(g_hwnd_input), IDC_DELETE), NULL,
        width - scale(MARGIN) - delete_width, height - scale(MARGIN) - scale(BUTTON_HEIGHT),
        delete_width, scale(BUTTON_HEIGHT), SWP_LAYOUT);

    int list_width = width - scale(MARGIN) * 2;
    ListView_SetColumnWidth(g_hwnd_list, 0, list_width - scale(DUE_COL_WIDTH + 10));
    ListView_SetColumnWidth(g_hwnd_list, 1, scale(DUE_COL_WIDTH));
}

static void layout_study_controls(int width, int height, int top) {
    int content_width = width - scale(MARGIN) * 2;
    int timer_height = scale(80);
    int status_height = scale(28);
    int btn_y = height - scale(MARGIN) - scale(BUTTON_HEIGHT);

    SetWindowPos(g_hwnd_study_input, NULL, scale(MARGIN), top, content_width, scale(INPUT_HEIGHT), SWP_LAYOUT);

    int timer_top = top + scale(INPUT_HEIGHT + GAP + 40);
    SetWindowPos(g_hwnd_study_timer, NULL, scale(MARGIN), timer_top, content_width, timer_height, SWP_LAYOUT);
    SetWindowPos(g_hwnd_study_status, NULL, scale(MARGIN), timer_top + timer_height + scale(8),
        content_width, status_height, SWP_LAYOUT);

    int start_width = scale(140);
    int hist_width = scale(130);
    SetWindowPos(g_hwnd_study_start, NULL,
        scale(MARGIN), btn_y, start_width, scale(BUTTON_HEIGHT), SWP_LAYOUT);
    SetWindowPos(g_hwnd_study_history, NULL,
        width - scale(MARGIN) - hist_width, btn_y, hist_width, scale(BUTTON_HEIGHT), SWP_LAYOUT);
}

static void layout_history_controls(int width, int height, int top) {
    int nav_btn = scale(72);
    int date_width = scale(150);
    int row_height = scale(INPUT_HEIGHT);
    int total_top = top + row_height + scale(GAP);
    int list_top = total_top + scale(28) + scale(GAP);
    int list_height = height - list_top - scale(BOTTOM_HEIGHT + MARGIN);

    if (list_height < scale(80)) {
        list_height = scale(80);
    }

    int x = scale(MARGIN);
    SetWindowPos(g_hwnd_hist_back, NULL, x, top, nav_btn, row_height, SWP_LAYOUT);
    x += nav_btn + scale(GAP);
    SetWindowPos(g_hwnd_hist_prev, NULL, x, top, nav_btn, row_height, SWP_LAYOUT);
    x += nav_btn + scale(GAP);
    SetWindowPos(g_hwnd_hist_date, NULL, x, top + scale(2), date_width, row_height, SWP_LAYOUT);
    x += date_width + scale(GAP);
    SetWindowPos(g_hwnd_hist_next, NULL, x, top, nav_btn, row_height, SWP_LAYOUT);

    SetWindowPos(g_hwnd_hist_total, NULL, scale(MARGIN), total_top, width - scale(MARGIN) * 2, scale(24), SWP_LAYOUT);
    SetWindowPos(g_hwnd_hist_list, NULL, scale(MARGIN), list_top, width - scale(MARGIN) * 2, list_height, SWP_LAYOUT);

    int delete_width = scale(130);
    int week_width = scale(130);
    int btn_y = height - scale(MARGIN) - scale(BUTTON_HEIGHT);
    SetWindowPos(g_hwnd_hist_week, NULL,
        scale(MARGIN), btn_y, week_width, scale(BUTTON_HEIGHT), SWP_LAYOUT);
    SetWindowPos(g_hwnd_hist_delete, NULL,
        width - scale(MARGIN) - delete_width, btn_y,
        delete_width, scale(BUTTON_HEIGHT), SWP_LAYOUT);

    int list_width = width - scale(MARGIN) * 2;
    ListView_SetColumnWidth(g_hwnd_hist_list, 0, scale(150));
    ListView_SetColumnWidth(g_hwnd_hist_list, 1, scale(100));
    ListView_SetColumnWidth(g_hwnd_hist_list, 2, list_width - scale(260));
}

static void layout_week_controls(int width, int height, int top) {
    (void)width;
    (void)height;
    int nav_btn = scale(72);
    int label_width = scale(170);
    int row_height = scale(INPUT_HEIGHT);
    int x = scale(MARGIN);

    SetWindowPos(g_hwnd_week_back, NULL, x, top, nav_btn, row_height, SWP_LAYOUT);
    x += nav_btn + scale(GAP);
    SetWindowPos(g_hwnd_week_prev, NULL, x, top, nav_btn, row_height, SWP_LAYOUT);
    x += nav_btn + scale(GAP);
    SetWindowPos(g_hwnd_week_label, NULL, x, top + scale(6), label_width, row_height, SWP_LAYOUT);
    x += label_width + scale(GAP);
    SetWindowPos(g_hwnd_week_next, NULL, x, top, nav_btn, row_height, SWP_LAYOUT);
}

static void layout_controls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    if (g_view == VIEW_TODO) {
        end_inline_edit(true);
    }

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int tab_top = scale(HEADER_HEIGHT + 4);
    int tab_width = scale(80);
    int content_top = scale(HEADER_HEIGHT + TAB_HEIGHT + MARGIN);

    SetWindowPos(g_hwnd_tab_todo, NULL, scale(MARGIN), tab_top, tab_width, scale(TAB_HEIGHT), SWP_LAYOUT);
    SetWindowPos(g_hwnd_tab_study, NULL, scale(MARGIN) + tab_width + scale(4), tab_top, tab_width, scale(TAB_HEIGHT), SWP_LAYOUT);

    if (g_view == VIEW_TODO) {
        layout_todo_controls(width, height, content_top);
    } else if (g_view == VIEW_STUDY) {
        layout_study_controls(width, height, content_top);
    } else if (g_view == VIEW_HISTORY) {
        layout_history_controls(width, height, content_top);
    } else {
        layout_week_controls(width, height, content_top);
    }
}

static void update_view_visibility(void) {
    bool todo = (g_view == VIEW_TODO);
    bool study = (g_view == VIEW_STUDY);
    bool history = (g_view == VIEW_HISTORY);
    bool week = (g_view == VIEW_WEEK);

    set_visible(g_hwnd_input, todo);
    set_visible(g_hwnd_due_check, todo);
    set_visible(g_hwnd_due_date, todo);
    set_visible(GetDlgItem(GetParent(g_hwnd_input), IDC_ADD), todo);
    set_visible(g_hwnd_list, todo);
    set_visible(GetDlgItem(GetParent(g_hwnd_input), IDC_DELETE), todo);

    set_visible(g_hwnd_study_input, study);
    set_visible(g_hwnd_study_timer, study);
    set_visible(g_hwnd_study_status, study);
    set_visible(g_hwnd_study_start, study);
    set_visible(g_hwnd_study_history, study);

    set_visible(g_hwnd_hist_back, history);
    set_visible(g_hwnd_hist_prev, history);
    set_visible(g_hwnd_hist_next, history);
    set_visible(g_hwnd_hist_date, history);
    set_visible(g_hwnd_hist_total, history);
    set_visible(g_hwnd_hist_list, history);
    set_visible(g_hwnd_hist_delete, history);
    set_visible(g_hwnd_hist_week, history);

    set_visible(g_hwnd_week_back, week);
    set_visible(g_hwnd_week_prev, week);
    set_visible(g_hwnd_week_next, week);
    set_visible(g_hwnd_week_label, week);

    set_visible(g_hwnd_tab_todo, true);
    set_visible(g_hwnd_tab_study, true);
}

static void update_study_clock_timer(HWND hwnd) {
    if (g_study_running || g_view == VIEW_STUDY) {
        SetTimer(hwnd, TIMER_STUDY_ID, 1000, NULL);
    } else {
        KillTimer(hwnd, TIMER_STUDY_ID);
    }
}

static void switch_view(HWND hwnd, AppView view) {
    AppView prev = g_view;
    g_view = view;
    update_view_visibility();
    layout_controls(hwnd);
    update_study_clock_timer(hwnd);

    if (view == VIEW_TODO) {
        SetWindowTextW(hwnd, L"Todo List");
        SetFocus(g_hwnd_input);
    } else if (view == VIEW_STUDY) {
        SetWindowTextW(hwnd, L"Work Timer");
        update_study_timer_display();
        SetFocus(g_hwnd_study_input);
    } else if (view == VIEW_HISTORY) {
        SetWindowTextW(hwnd, L"Work History");
        g_hist_selected_row = -1;
        refresh_history();
        SetFocus(g_hwnd_hist_list);
    } else {
        SetWindowTextW(hwnd, L"Week History");
        g_week_offset = 0;
        update_week_label();
        SetFocus(g_hwnd_week_prev);
    }

    if (prev == VIEW_WEEK || view == VIEW_WEEK) {
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
    } else {
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

static void sync_selection_visual(void) {
    int count = ListView_GetItemCount(g_hwnd_list);

    g_programmatic_select = true;
    for (int i = 0; i < count; i++) {
        if (i == g_selected_index) {
            ListView_SetItemState(g_hwnd_list, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        } else {
            ListView_SetItemState(g_hwnd_list, i, 0, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
    g_programmatic_select = false;
}

static void clear_list_selection(void) {
    g_selected_index = -1;
    sync_selection_visual();
}

static void select_list_item(int index) {
    if (index < 0 || index >= g_todos.count) {
        return;
    }
    g_selected_index = index;
    sync_selection_visual();
}

static void toggle_list_item_selection(int index) {
    if (index < 0 || index >= g_todos.count) {
        return;
    }
    if (g_selected_index == index) {
        clear_list_selection();
    } else {
        select_list_item(index);
    }
}

static const char *deadline_display(const TodoItem *item, char *buf, int bufsize) {
    if (!todo_deadline_is_valid(item->deadline)) {
        return "-";
    }
    strncpy(buf, item->deadline, (size_t)bufsize - 1);
    buf[bufsize - 1] = '\0';
    return buf;
}

static void refresh_list(void) {
    int keep_selected = g_selected_index;

    end_inline_edit(false);

    g_updating_list = true;
    ListView_DeleteAllItems(g_hwnd_list);

    for (int i = 0; i < g_todos.count; i++) {
        wchar_t wtext[TODO_MAX_TEXT];
        wchar_t wdue[TODO_MAX_DEADLINE];
        char due_buf[TODO_MAX_DEADLINE];

        utf8_to_wide(g_todos.items[i].text, wtext, TODO_MAX_TEXT);
        utf8_to_wide(deadline_display(&g_todos.items[i], due_buf, sizeof(due_buf)), wdue, TODO_MAX_DEADLINE);

        LVITEMW item;
        memset(&item, 0, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.pszText = wtext;
        SendMessageW(g_hwnd_list, LVM_INSERTITEMW, 0, (LPARAM)&item);

        LVITEMW sub;
        memset(&sub, 0, sizeof(sub));
        sub.mask = LVIF_TEXT;
        sub.iItem = i;
        sub.iSubItem = 1;
        sub.pszText = wdue;
        SendMessageW(g_hwnd_list, LVM_SETITEMW, 0, (LPARAM)&sub);

        ListView_SetCheckState(g_hwnd_list, i, g_todos.items[i].done);
    }

    g_updating_list = false;

    if (keep_selected >= g_todos.count) {
        keep_selected = -1;
    }
    g_selected_index = keep_selected;
    sync_selection_visual();

    InvalidateRect(GetParent(g_hwnd_list), NULL, FALSE);
}

static bool save_todos(void) {
    return todo_save(&g_todos, g_data_path);
}

static bool save_study_log(void) {
    return study_save(&g_study_log, g_study_path);
}

static void update_due_date_enabled(void) {
    BOOL enabled = (SendMessageW(g_hwnd_due_check, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(g_hwnd_due_date, enabled);
}

static void get_deadline_from_ui(char *buf, int bufsize) {
    buf[0] = '\0';
    if (SendMessageW(g_hwnd_due_check, BM_GETCHECK, 0, 0) != BST_CHECKED) {
        return;
    }

    SYSTEMTIME st;
    if (SendMessageW(g_hwnd_due_date, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) != GDT_VALID) {
        return;
    }

    snprintf(buf, (size_t)bufsize, "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
}

static void add_task_from_input(HWND hwnd) {
    wchar_t wtext[TODO_MAX_TEXT];
    char text[TODO_MAX_TEXT];
    char deadline[TODO_MAX_DEADLINE];

    GetWindowTextW(g_hwnd_input, wtext, TODO_MAX_TEXT);
    wide_to_utf8(wtext, text, sizeof(text));
    get_deadline_from_ui(deadline, sizeof(deadline));

    if (!todo_add(&g_todos, text, deadline[0] ? deadline : NULL)) {
        return;
    }

    SetWindowTextW(g_hwnd_input, L"");
    SendMessageW(g_hwnd_due_check, BM_SETCHECK, BST_UNCHECKED, 0);
    update_due_date_enabled();
    refresh_list();
    save_todos();
    InvalidateRect(hwnd, NULL, FALSE);
    SetFocus(g_hwnd_input);
}

static int get_target_list_index(void) {
    if (g_selected_index < 0 || g_selected_index >= g_todos.count) {
        return -1;
    }
    return g_selected_index;
}

static void delete_selected_task(HWND hwnd) {
    int index = get_target_list_index();
    if (index < 0) {
        return;
    }

    if (!todo_remove(&g_todos, index)) {
        return;
    }

    refresh_list();
    save_todos();

    if (g_todos.count > 0) {
        int next = index < g_todos.count ? index : g_todos.count - 1;
        select_list_item(next);
    } else {
        clear_list_selection();
    }

    InvalidateRect(hwnd, NULL, FALSE);
    SetFocus(g_hwnd_list);
}

static void begin_inline_edit(int index) {
    if (index < 0 || index >= g_todos.count) {
        return;
    }

    RECT rc;
    if (!ListView_GetItemRect(g_hwnd_list, index, &rc, LVIR_LABEL)) {
        return;
    }

    int col_right = ListView_GetColumnWidth(g_hwnd_list, 0);
    if (rc.right > col_right) {
        rc.right = col_right;
    }

    wchar_t wtext[TODO_MAX_TEXT];
    utf8_to_wide(g_todos.items[index].text, wtext, TODO_MAX_TEXT);

    g_editing_index = index;
    SetWindowTextW(g_hwnd_edit_box, wtext);
    SetWindowPos(g_hwnd_edit_box, HWND_TOP,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_SHOWWINDOW);
    SendMessageW(g_hwnd_edit_box, EM_SETSEL, 0, (LPARAM)-1);
    SetFocus(g_hwnd_edit_box);
}

static void end_inline_edit(bool commit) {
    if (g_editing_index < 0) {
        return;
    }

    int index = g_editing_index;
    g_editing_index = -1;

    if (commit) {
        wchar_t wtext[TODO_MAX_TEXT];
        char text[TODO_MAX_TEXT];

        GetWindowTextW(g_hwnd_edit_box, wtext, TODO_MAX_TEXT);
        wide_to_utf8(wtext, text, sizeof(text));

        if (todo_set_text(&g_todos, index, text)) {
            refresh_list();
            save_todos();
        }
    }

    ShowWindow(g_hwnd_edit_box, SW_HIDE);
}

static WNDPROC g_edit_box_orig_proc;
static WNDPROC g_study_timer_orig_proc;

static void paint_study_timer(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        return;
    }

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ old_bmp = SelectObject(mem, bmp);

    FillRect(mem, &rc, g_brush_bg);
    SetBkMode(mem, TRANSPARENT);
    HGDIOBJ old_font = SelectObject(mem, g_font_timer);
    SetTextColor(mem, CLR_TITLE);
    DrawTextW(mem, g_study_timer_last, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(mem, old_font);

    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);

    SelectObject(mem, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    (void)hwnd;
}

static LRESULT CALLBACK study_timer_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paint_study_timer(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return CallWindowProcW(g_study_timer_orig_proc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK edit_box_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            end_inline_edit(true);
            SetFocus(g_hwnd_list);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            end_inline_edit(false);
            SetFocus(g_hwnd_list);
            return 0;
        }
        break;

    case WM_KILLFOCUS:
        end_inline_edit(true);
        break;

    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    }

    return CallWindowProcW(g_edit_box_orig_proc, hwnd, msg, wParam, lParam);
}

static void update_study_timer_display(void) {
    wchar_t buf[32];

    if (g_study_running) {
        int elapsed = (int)((GetTickCount() - g_study_start_tick) / 1000);
        _snwprintf(buf, 32, L"%02d:%02d:%02d",
            elapsed / 3600, (elapsed % 3600) / 60, elapsed % 60);
    } else {
        SYSTEMTIME now;
        GetLocalTime(&now);
        _snwprintf(buf, 32, L"%02d:%02d:%02d", now.wHour, now.wMinute, now.wSecond);
    }

    if (wcscmp(buf, g_study_timer_last) == 0) {
        return;
    }
    wcscpy(g_study_timer_last, buf);
    if (IsWindow(g_hwnd_study_timer)) {
        InvalidateRect(g_hwnd_study_timer, NULL, FALSE);
    }
}

static void start_study(HWND hwnd) {
    wchar_t wtask[STUDY_MAX_TEXT];
    char task[STUDY_MAX_TEXT];

    GetWindowTextW(g_hwnd_study_input, wtask, STUDY_MAX_TEXT);
    wide_to_utf8(wtask, task, sizeof(task));
    if (task[0] == '\0') {
        SetWindowTextW(g_hwnd_study_status, L"Please enter what you are working on.");
        return;
    }

    strncpy(g_study_task, task, STUDY_MAX_TEXT - 1);
    g_study_task[STUDY_MAX_TEXT - 1] = '\0';
    GetLocalTime(&g_study_start_st);
    g_study_start_tick = GetTickCount();
    g_study_running = true;
    g_study_timer_last[0] = L'\0';

    SetWindowTextW(g_hwnd_study_start, L"Stop Work");
    EnableWindow(g_hwnd_study_input, FALSE);
    wcscpy(g_study_timer_last, L"00:00:00");
    InvalidateRect(g_hwnd_study_timer, NULL, FALSE);
    update_study_clock_timer(hwnd);

    wchar_t status[STUDY_MAX_TEXT + 32];
    utf8_to_wide(g_study_task, wtask, STUDY_MAX_TEXT);
    _snwprintf(status, STUDY_MAX_TEXT + 32, L"Working: %s", wtask);
    SetWindowTextW(g_hwnd_study_status, status);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void stop_study(HWND hwnd) {
    if (!g_study_running) {
        return;
    }

    KillTimer(hwnd, TIMER_STUDY_ID);
    g_study_running = false;
    g_study_timer_last[0] = L'\0';

    SYSTEMTIME end_st;
    GetLocalTime(&end_st);

    int duration = (int)((GetTickCount() - g_study_start_tick) / 1000);

    SetWindowTextW(g_hwnd_study_start, L"Start Work");
    EnableWindow(g_hwnd_study_input, TRUE);

    if (duration < STUDY_MIN_SAVE_SEC) {
        SetWindowTextW(g_hwnd_study_status, L"Under 1 min — session not saved.");
        if (g_view == VIEW_STUDY) {
            update_study_clock_timer(hwnd);
            update_study_timer_display();
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    study_add_session_span(&g_study_log, &g_study_start_st, &end_st, duration, g_study_task);
    save_study_log();

    if (g_view == VIEW_STUDY) {
        update_study_clock_timer(hwnd);
        update_study_timer_display();
    }

    wchar_t dur[32];
    study_format_duration(duration, dur, 32);
    wchar_t status[96];
    _snwprintf(status, 96, L"Session saved (%s).", dur);
    SetWindowTextW(g_hwnd_study_status, status);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void set_history_date_from_picker(void) {
    SYSTEMTIME st;
    if (SendMessageW(g_hwnd_hist_date, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) != GDT_VALID) {
        return;
    }
    study_systemtime_to_date(&st, g_history_date, sizeof(g_history_date));
}

static void set_history_date_picker(const char *date) {
    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    st.wYear = (WORD)((date[0] - '0') * 1000 + (date[1] - '0') * 100
            + (date[2] - '0') * 10 + (date[3] - '0'));
    st.wMonth = (WORD)((date[5] - '0') * 10 + (date[6] - '0'));
    st.wDay = (WORD)((date[8] - '0') * 10 + (date[9] - '0'));
    SendMessageW(g_hwnd_hist_date, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
    strncpy(g_history_date, date, STUDY_MAX_DATE - 1);
    g_history_date[STUDY_MAX_DATE - 1] = '\0';
}

static void refresh_history(void) {
    set_history_date_from_picker();

    int total = study_total_seconds_for_date(&g_study_log, g_history_date);
    int count = 0;
    for (int i = 0; i < g_study_log.count; i++) {
        if (strcmp(g_study_log.items[i].date, g_history_date) == 0) {
            count++;
        }
    }

    wchar_t dur[32];
    study_format_duration(total, dur, 32);
    wchar_t total_buf[64];
    _snwprintf(total_buf, 64, L"Total: %s  (%d sessions)", dur, count);
    SetWindowTextW(g_hwnd_hist_total, total_buf);

    int keep_row = g_hist_selected_row;
    ListView_DeleteAllItems(g_hwnd_hist_list);

    int row = 0;
    for (int i = 0; i < g_study_log.count; i++) {
        StudySession *session = &g_study_log.items[i];
        if (strcmp(session->date, g_history_date) != 0) {
            continue;
        }

        wchar_t wstart[8], wend[8], wrange[24], wdur[32], wtask[STUDY_MAX_TEXT];

        study_format_time_hm(session->start_time, wstart, 8);
        study_format_time_hm(session->end_time, wend, 8);
        _snwprintf(wrange, 24, L"%s - %s", wstart, wend);
        study_format_duration(session->duration_sec, wdur, 32);
        utf8_to_wide(session->task, wtask, STUDY_MAX_TEXT);

        LVITEMW item;
        memset(&item, 0, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.pszText = wrange;
        item.lParam = (LPARAM)i;
        SendMessageW(g_hwnd_hist_list, LVM_INSERTITEMW, 0, (LPARAM)&item);

        LVITEMW sub;
        memset(&sub, 0, sizeof(sub));
        sub.mask = LVIF_TEXT;
        sub.iItem = row;
        sub.iSubItem = 1;
        sub.pszText = wdur;
        SendMessageW(g_hwnd_hist_list, LVM_SETITEMW, 0, (LPARAM)&sub);

        sub.iSubItem = 2;
        sub.pszText = wtask;
        SendMessageW(g_hwnd_hist_list, LVM_SETITEMW, 0, (LPARAM)&sub);
        row++;
    }

    if (keep_row >= row) {
        keep_row = -1;
    }
    g_hist_selected_row = keep_row;
    sync_hist_selection_visual();
}

static void sync_hist_selection_visual(void) {
    int count = ListView_GetItemCount(g_hwnd_hist_list);

    g_hist_programmatic_select = true;
    for (int i = 0; i < count; i++) {
        if (i == g_hist_selected_row) {
            ListView_SetItemState(g_hwnd_hist_list, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        } else {
            ListView_SetItemState(g_hwnd_hist_list, i, 0, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
    g_hist_programmatic_select = false;
}

static void clear_hist_selection(void) {
    g_hist_selected_row = -1;
    sync_hist_selection_visual();
}

static void select_hist_row(int row) {
    if (row < 0 || row >= ListView_GetItemCount(g_hwnd_hist_list)) {
        return;
    }
    g_hist_selected_row = row;
    sync_hist_selection_visual();
}

static void toggle_hist_row_selection(int row) {
    if (row < 0 || row >= ListView_GetItemCount(g_hwnd_hist_list)) {
        return;
    }
    if (g_hist_selected_row == row) {
        clear_hist_selection();
    } else {
        select_hist_row(row);
    }
}

static void delete_selected_history(HWND hwnd) {
    if (g_hist_selected_row < 0) {
        return;
    }

    LVITEMW lv_item;
    memset(&lv_item, 0, sizeof(lv_item));
    lv_item.mask = LVIF_PARAM;
    lv_item.iItem = g_hist_selected_row;
    if (!SendMessageW(g_hwnd_hist_list, LVM_GETITEMW, 0, (LPARAM)&lv_item)) {
        return;
    }

    int log_index = (int)lv_item.lParam;
    if (!study_remove_at(&g_study_log, log_index)) {
        return;
    }

    save_study_log();

    int next = g_hist_selected_row;
    refresh_history();

    int count = ListView_GetItemCount(g_hwnd_hist_list);
    if (count > 0) {
        if (next >= count) {
            next = count - 1;
        }
        select_hist_row(next);
    } else {
        clear_hist_selection();
    }

    InvalidateRect(hwnd, NULL, FALSE);
    SetFocus(g_hwnd_hist_list);
}

static void history_navigate_days(int delta) {
    char next[STUDY_MAX_DATE];
    if (!study_date_add_days(g_history_date, delta, next, sizeof(next))) {
        return;
    }
    set_history_date_picker(next);
    refresh_history();
}

static void update_week_label(void) {
    char dates[7][STUDY_MAX_DATE];
    int totals[7];
    study_week_totals(&g_study_log, g_week_offset, dates, totals);
    (void)totals;

    wchar_t label[48];
    _snwprintf(label, 48, L"%c%c-%c%c ~ %c%c-%c%c",
        dates[0][5], dates[0][6], dates[0][8], dates[0][9],
        dates[6][5], dates[6][6], dates[6][8], dates[6][9]);
    SetWindowTextW(g_hwnd_week_label, label);
    EnableWindow(g_hwnd_week_next, g_week_offset < 0);
}

static void week_navigate(HWND hwnd, int delta) {
    if (delta > 0 && g_week_offset >= 0) {
        return;
    }

    g_week_offset += delta;
    if (g_week_offset > 0) {
        g_week_offset = 0;
    }

    update_week_label();
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

static void paint_header_dc(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    RECT header = rc;
    header.bottom = scale(HEADER_HEIGHT);
    FillRect(hdc, &header, g_brush_surface);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(230, 230, 230));
    HPEN old_pen = SelectObject(hdc, border);
    MoveToEx(hdc, 0, scale(HEADER_HEIGHT) - 1, NULL);
    LineTo(hdc, rc.right, scale(HEADER_HEIGHT) - 1);
    SelectObject(hdc, old_pen);
    DeleteObject(border);

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_font_title);
    SetTextColor(hdc, CLR_TITLE);

    const wchar_t *title = L"Todo List";
    wchar_t stats[64] = L"";

    if (g_view == VIEW_TODO) {
        wsprintfW(stats, L"%d pending", todo_count_pending(&g_todos));
    } else if (g_view == VIEW_STUDY) {
        title = L"Work Timer";
        if (g_study_running) {
            wcscpy(stats, L"Recording...");
        }
    } else if (g_view == VIEW_HISTORY) {
        title = L"Work History";
    } else {
        title = L"Week History";
        char dates[7][STUDY_MAX_DATE];
        int totals[7];
        study_week_totals(&g_study_log, g_week_offset, dates, totals);
        int week_total = 0;
        for (int i = 0; i < 7; i++) {
            week_total += totals[i];
        }
        wchar_t dur[32];
        study_format_duration(week_total, dur, 32);
        _snwprintf(stats, 64, L"Week total: %s", dur);
    }

    RECT title_rc = header;
    title_rc.left = scale(MARGIN);
    title_rc.right = rc.right / 2;
    DrawTextW(hdc, title, -1, &title_rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    if (stats[0] != L'\0') {
        SelectObject(hdc, g_font_normal);
        SetTextColor(hdc, CLR_MUTED);
        RECT stats_rc = header;
        stats_rc.right = rc.right - scale(MARGIN);
        DrawTextW(hdc, stats, -1, &stats_rc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    }
}

static void paint_week_chart(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    int content_top = scale(HEADER_HEIGHT + TAB_HEIGHT + MARGIN);
    int chart_top = content_top + scale(INPUT_HEIGHT) + scale(GAP + 6);
    int chart_bottom = rc.bottom - scale(MARGIN);

    RECT chart_area;
    chart_area.left = scale(MARGIN);
    chart_area.right = rc.right - scale(MARGIN);
    chart_area.top = chart_top;
    chart_area.bottom = chart_bottom;
    if (chart_area.bottom - chart_area.top < scale(120)) {
        return;
    }

    FillRect(hdc, &chart_area, g_brush_surface);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(230, 230, 230));
    HPEN old_pen = SelectObject(hdc, border);
    Rectangle(hdc, chart_area.left, chart_area.top, chart_area.right, chart_area.bottom);
    SelectObject(hdc, old_pen);
    DeleteObject(border);

    char dates[7][STUDY_MAX_DATE];
    int totals[7];
    study_week_totals(&g_study_log, g_week_offset, dates, totals);

    static const wchar_t *day_names[] = {
        L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat", L"Sun"
    };

    int max_total = 3600;
    for (int i = 0; i < 7; i++) {
        if (totals[i] > max_total) {
            max_total = totals[i];
        }
    }

    int value_label_h = scale(22);
    int bottom_label_h = scale(52);
    int pad = scale(16);
    int inner_top = chart_area.top + pad + value_label_h;
    int inner_bottom = chart_area.bottom - pad - bottom_label_h;
    int inner_height = inner_bottom - inner_top;
    if (inner_height < scale(8)) {
        return;
    }

    int chart_width = chart_area.right - chart_area.left - pad * 2;
    int gap = scale(10);
    int bar_width = (chart_width - gap * 6) / 7;
    if (bar_width < scale(12)) {
        bar_width = scale(12);
    }

    HBRUSH bar_brush = CreateSolidBrush(CLR_ACCENT);
    HBRUSH bar_empty = CreateSolidBrush(RGB(235, 235, 235));
    SelectObject(hdc, g_font_normal);
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < 7; i++) {
        int x = chart_area.left + pad + i * (bar_width + gap);
        int bar_h = 0;
        if (max_total > 0 && totals[i] > 0) {
            bar_h = (int)((LONGLONG)totals[i] * inner_height / max_total);
            if (bar_h < scale(4)) {
                bar_h = scale(4);
            }
            if (bar_h > inner_height) {
                bar_h = inner_height;
            }
        }

        if (totals[i] > 0) {
            wchar_t dur[16];
            study_format_duration(totals[i], dur, 16);
            SetTextColor(hdc, CLR_TITLE);
            RECT dur_rc;
            dur_rc.left = x;
            dur_rc.right = x + bar_width;
            dur_rc.top = chart_area.top + pad;
            dur_rc.bottom = dur_rc.top + value_label_h;
            DrawTextW(hdc, dur, -1, &dur_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        RECT bar;
        bar.left = x;
        bar.right = x + bar_width;
        bar.bottom = inner_bottom;
        bar.top = inner_bottom - bar_h;

        FillRect(hdc, &bar, totals[i] > 0 ? bar_brush : bar_empty);

        wchar_t date_label[16];
        _snwprintf(date_label, 16, L"%c%c-%c%c",
            dates[i][5], dates[i][6], dates[i][8], dates[i][9]);

        SetTextColor(hdc, CLR_TITLE);
        RECT day_rc;
        day_rc.left = x;
        day_rc.right = x + bar_width;
        day_rc.top = inner_bottom + scale(6);
        day_rc.bottom = day_rc.top + scale(20);
        DrawTextW(hdc, day_names[i], -1, &day_rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        SetTextColor(hdc, CLR_MUTED);
        RECT date_rc = day_rc;
        date_rc.top = day_rc.bottom + scale(2);
        date_rc.bottom = date_rc.top + scale(18);
        DrawTextW(hdc, date_label, -1, &date_rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    DeleteObject(bar_brush);
    DeleteObject(bar_empty);
    (void)hwnd;
}

static void paint_content_background(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    RECT content = rc;
    content.top = scale(HEADER_HEIGHT);
    FillRect(hdc, &content, g_brush_bg);
    (void)hwnd;
}

static void paint_header(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    paint_header_dc(hwnd, hdc);
    if (g_view == VIEW_WEEK) {
        paint_week_chart(hwnd, hdc);
    } else {
        paint_content_background(hwnd, hdc);
    }
    EndPaint(hwnd, &ps);
}

static void draw_empty_list_message(HDC hdc, const RECT *rc) {
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_font_normal);
    SetTextColor(hdc, CLR_MUTED);
    DrawTextW(hdc, L"No tasks yet. Add one above.", -1, (RECT *)rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

static LRESULT on_list_custom_draw(LPNMLVCUSTOMDRAW lvcd) {
    switch (lvcd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        if (g_todos.count == 0) {
            draw_empty_list_message(lvcd->nmcd.hdc, &lvcd->nmcd.rc);
            return CDRF_SKIPDEFAULT;
        }
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT:
        return CDRF_NOTIFYSUBITEMDRAW;

    case CDDS_SUBITEMPREPAINT: {
        int index = (int)lvcd->nmcd.dwItemSpec;
        if (index < 0 || index >= g_todos.count) {
            return CDRF_DODEFAULT;
        }

        TodoItem *item = &g_todos.items[index];
        bool selected = (index == g_selected_index);
        bool overdue = !item->done && todo_deadline_is_overdue(item->deadline);

        if (selected) {
            lvcd->clrTextBk = CLR_SELECT_BG;
            lvcd->clrText = CLR_SELECT_FG;
        } else {
            lvcd->clrTextBk = CLR_SURFACE;
            if (item->done) {
                lvcd->clrText = CLR_DONE;
            } else if (overdue && lvcd->iSubItem == 1) {
                lvcd->clrText = CLR_OVERDUE;
            } else {
                lvcd->clrText = CLR_TITLE;
            }
        }

        if (item->done) {
            SelectObject(lvcd->nmcd.hdc, g_font_strike);
            return CDRF_NEWFONT;
        }
        SelectObject(lvcd->nmcd.hdc, g_font_normal);
        return CDRF_NEWFONT;
    }
    }

    return CDRF_DODEFAULT;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_dpi = get_window_dpi(hwnd);

        g_brush_bg = CreateSolidBrush(CLR_BG);
        g_brush_surface = CreateSolidBrush(CLR_SURFACE);
        create_fonts();

        g_hwnd_tab_todo = CreateWindowExW(
            0, L"BUTTON", L"Todo",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_TAB_TODO, NULL, NULL);

        g_hwnd_tab_study = CreateWindowExW(
            0, L"BUTTON", L"Work",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_TAB_STUDY, NULL, NULL);

        g_hwnd_input = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_INPUT, NULL, NULL);
        SendMessageW(g_hwnd_input, EM_SETLIMITTEXT, TODO_MAX_TEXT - 1, 0);

        g_hwnd_due_check = CreateWindowExW(
            0, L"BUTTON", L"Due",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_DUE_CHECK, NULL, NULL);

        g_hwnd_due_date = CreateWindowExW(
            0, DATETIMEPICK_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT,
            0, 0, 0, 0, hwnd, (HMENU)IDC_DUE_DATE, NULL, NULL);

        CreateWindowExW(
            0, L"BUTTON", L"Add",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_ADD, NULL, NULL);

        g_hwnd_list = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd, (HMENU)IDC_LIST, NULL, NULL);

        ListView_SetExtendedListViewStyle(
            g_hwnd_list,
            LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(g_hwnd_list, CLR_SURFACE);
        ListView_SetTextBkColor(g_hwnd_list, CLR_SURFACE);

        LVCOLUMNW col;
        memset(&col, 0, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = L"Task";
        col.cx = 320;
        col.iSubItem = 0;
        SendMessageW(g_hwnd_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);
        col.pszText = L"Due";
        col.cx = 110;
        col.iSubItem = 1;
        SendMessageW(g_hwnd_list, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);

        g_hwnd_edit_box = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 0, 0, g_hwnd_list, (HMENU)IDC_EDIT_BOX, NULL, NULL);
        SendMessageW(g_hwnd_edit_box, EM_SETLIMITTEXT, TODO_MAX_TEXT - 1, 0);
        g_edit_box_orig_proc = (WNDPROC)SetWindowLongPtrW(g_hwnd_edit_box, GWLP_WNDPROC, (LONG_PTR)edit_box_proc);

        CreateWindowExW(
            0, L"BUTTON", L"Delete Selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_DELETE, NULL, NULL);

        g_hwnd_study_input = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STUDY_INPUT, NULL, NULL);
        SendMessageW(g_hwnd_study_input, EM_SETLIMITTEXT, STUDY_MAX_TEXT - 1, 0);

        g_hwnd_study_timer = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | SS_CENTER | SS_NOPREFIX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STUDY_TIMER, NULL, NULL);
        g_study_timer_orig_proc = (WNDPROC)SetWindowLongPtrW(
            g_hwnd_study_timer, GWLP_WNDPROC, (LONG_PTR)study_timer_proc);

        g_hwnd_study_status = CreateWindowExW(
            0, L"STATIC", L"Enter a task and click Start Work.",
            WS_CHILD | SS_CENTER,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STUDY_STATUS, NULL, NULL);

        g_hwnd_study_start = CreateWindowExW(
            0, L"BUTTON", L"Start Work",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STUDY_START, NULL, NULL);

        g_hwnd_study_history = CreateWindowExW(
            0, L"BUTTON", L"View History",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STUDY_HISTORY, NULL, NULL);

        g_hwnd_hist_back = CreateWindowExW(
            0, L"BUTTON", L"Back",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_HIST_BACK, NULL, NULL);

        g_hwnd_hist_prev = CreateWindowExW(
            0, L"BUTTON", L"Prev",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_HIST_PREV, NULL, NULL);

        g_hwnd_hist_next = CreateWindowExW(
            0, L"BUTTON", L"Next",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_HIST_NEXT, NULL, NULL);

        g_hwnd_hist_date = CreateWindowExW(
            0, DATETIMEPICK_CLASSW, L"",
            WS_CHILD | DTS_SHORTDATEFORMAT,
            0, 0, 0, 0, hwnd, (HMENU)IDC_HIST_DATE, NULL, NULL);

        g_hwnd_hist_total = CreateWindowExW(
            0, L"STATIC", L"Total: 0 sec",
            WS_CHILD,
            0, 0, 0, 0, hwnd, (HMENU)IDC_HIST_TOTAL, NULL, NULL);

        g_hwnd_hist_list = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd, (HMENU)IDC_HIST_LIST, NULL, NULL);

        ListView_SetExtendedListViewStyle(g_hwnd_hist_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(g_hwnd_hist_list, CLR_SURFACE);
        ListView_SetTextBkColor(g_hwnd_hist_list, CLR_SURFACE);

        memset(&col, 0, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = L"Time";
        col.cx = 150;
        col.iSubItem = 0;
        SendMessageW(g_hwnd_hist_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);
        col.pszText = L"Duration";
        col.cx = 100;
        col.iSubItem = 1;
        SendMessageW(g_hwnd_hist_list, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);
        col.pszText = L"Task";
        col.cx = 300;
        col.iSubItem = 2;
        SendMessageW(g_hwnd_hist_list, LVM_INSERTCOLUMNW, 2, (LPARAM)&col);

        g_hwnd_hist_delete = CreateWindowExW(
            0, L"BUTTON", L"Delete Selected",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_HIST_DELETE, NULL, NULL);

        g_hwnd_hist_week = CreateWindowExW(
            0, L"BUTTON", L"Week History",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_HIST_WEEK, NULL, NULL);

        g_hwnd_week_back = CreateWindowExW(
            0, L"BUTTON", L"Back",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_WEEK_BACK, NULL, NULL);

        g_hwnd_week_prev = CreateWindowExW(
            0, L"BUTTON", L"Prev",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_WEEK_PREV, NULL, NULL);

        g_hwnd_week_label = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | SS_CENTER,
            0, 0, 0, 0, hwnd, (HMENU)IDC_WEEK_LABEL, NULL, NULL);

        g_hwnd_week_next = CreateWindowExW(
            0, L"BUTTON", L"Next",
            WS_CHILD | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_WEEK_NEXT, NULL, NULL);

        apply_fonts(hwnd);

        SYSTEMTIME today;
        GetLocalTime(&today);
        SendMessageW(g_hwnd_due_date, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&today);
        SendMessageW(g_hwnd_hist_date, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&today);
        EnableWindow(g_hwnd_due_date, FALSE);

        todo_list_init(&g_todos);
        study_log_init(&g_study_log);

        if (!todo_get_data_path(g_data_path, MAX_PATH)) {
            wcscpy(g_data_path, L"todos.dat");
        }
        if (!study_get_data_path(g_study_path, MAX_PATH)) {
            wcscpy(g_study_path, L"study.dat");
        }

        study_today_date(g_history_date, sizeof(g_history_date));
        g_selected_index = -1;
        g_programmatic_select = false;
        g_study_running = false;

        todo_load(&g_todos, g_data_path);
        study_load(&g_study_log, g_study_path);
        refresh_list();

        update_view_visibility();
        update_study_timer_display();
        SetWindowPos(hwnd, NULL, 0, 0, scale(WINDOW_WIDTH), scale(WINDOW_HEIGHT),
            SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
        layout_controls(hwnd);
        SetFocus(g_hwnd_input);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mmi = (MINMAXINFO *)lParam;
        get_min_window_size(hwnd, &mmi->ptMinTrackSize.x, &mmi->ptMinTrackSize.y);
        return 0;
    }

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            layout_controls(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            invalidate_child_controls(hwnd);
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_STUDY_ID && g_view == VIEW_STUDY) {
            update_study_timer_display();
        }
        return 0;

    case WM_DPICHANGED: {
        g_dpi = HIWORD(wParam);
        RECT *suggested = (RECT *)lParam;

        destroy_fonts();
        create_fonts();
        apply_fonts(hwnd);

        SetWindowPos(hwnd, NULL,
            suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        layout_controls(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wParam, &rc, g_brush_bg);
        return 1;
    }

    case WM_CTLCOLOREDIT:
        SetBkColor((HDC)wParam, CLR_SURFACE);
        SetTextColor((HDC)wParam, CLR_TITLE);
        return (LRESULT)g_brush_surface;
    case WM_CTLCOLORSTATIC: {
        HWND ctl = (HWND)lParam;
        int ctl_id = GetDlgCtrlID(ctl);
        if (ctl_id == IDC_STUDY_TIMER || ctl_id == IDC_STUDY_STATUS) {
            SetBkColor((HDC)wParam, CLR_BG);
            SetTextColor((HDC)wParam, CLR_TITLE);
            return (LRESULT)g_brush_bg;
        }
        SetBkColor((HDC)wParam, CLR_SURFACE);
        SetTextColor((HDC)wParam, CLR_TITLE);
        return (LRESULT)g_brush_surface;
    }

    case WM_PAINT:
        paint_header(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_TAB_TODO:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch_view(hwnd, VIEW_TODO);
            }
            return 0;
        case IDC_TAB_STUDY:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch_view(hwnd, VIEW_STUDY);
            }
            return 0;
        case IDC_DUE_CHECK:
            if (HIWORD(wParam) == BN_CLICKED) {
                update_due_date_enabled();
            }
            return 0;
        case IDC_ADD:
        case IDM_ADD:
            add_task_from_input(hwnd);
            return 0;
        case IDC_DELETE:
        case IDM_DELETE:
            delete_selected_task(hwnd);
            return 0;
        case IDC_STUDY_START:
            if (HIWORD(wParam) == BN_CLICKED) {
                if (g_study_running) {
                    stop_study(hwnd);
                } else {
                    start_study(hwnd);
                }
            }
            return 0;
        case IDC_STUDY_HISTORY:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch_view(hwnd, VIEW_HISTORY);
            }
            return 0;
        case IDC_HIST_BACK:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch_view(hwnd, VIEW_STUDY);
            }
            return 0;
        case IDC_HIST_PREV:
            if (HIWORD(wParam) == BN_CLICKED) {
                history_navigate_days(-1);
            }
            return 0;
        case IDC_HIST_NEXT:
            if (HIWORD(wParam) == BN_CLICKED) {
                history_navigate_days(1);
            }
            return 0;
        case IDC_HIST_DELETE:
            if (HIWORD(wParam) == BN_CLICKED) {
                delete_selected_history(hwnd);
            }
            return 0;
        case IDC_HIST_WEEK:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch_view(hwnd, VIEW_WEEK);
            }
            return 0;
        case IDC_WEEK_BACK:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch_view(hwnd, VIEW_HISTORY);
            }
            return 0;
        case IDC_WEEK_PREV:
            if (HIWORD(wParam) == BN_CLICKED) {
                week_navigate(hwnd, -1);
            }
            return 0;
        case IDC_WEEK_NEXT:
            if (HIWORD(wParam) == BN_CLICKED) {
                week_navigate(hwnd, 1);
            }
            return 0;
        }
        break;

    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lParam;

        if (hdr->idFrom == IDC_HIST_DATE && hdr->code == DTN_DATETIMECHANGE) {
            refresh_history();
            return 0;
        }

        if (hdr->idFrom == IDC_HIST_LIST) {
            if (hdr->code == LVN_ITEMCHANGING) {
                if (!g_hist_programmatic_select) {
                    LPNMLISTVIEW lv = (LPNMLISTVIEW)lParam;
                    if ((lv->uChanged & LVIF_STATE) && ((lv->uNewState ^ lv->uOldState) & LVIS_SELECTED)) {
                        return TRUE;
                    }
                }
                return 0;
            }
            if (hdr->code == LVN_KEYDOWN) {
                LPNMLVKEYDOWN key = (LPNMLVKEYDOWN)lParam;
                if (key->wVKey == VK_DELETE) {
                    delete_selected_history(hwnd);
                }
                return 0;
            }
            if (hdr->code == NM_CLICK) {
                LPNMITEMACTIVATE click = (LPNMITEMACTIVATE)lParam;
                LVHITTESTINFO ht;
                ht.pt = click->ptAction;
                int hit = ListView_HitTest(g_hwnd_hist_list, &ht);

                if (hit >= 0 && (ht.flags & (LVHT_ONITEMLABEL | LVHT_ONITEM))) {
                    toggle_hist_row_selection(hit);
                } else {
                    clear_hist_selection();
                }
                return 0;
            }
        }

        if (hdr->idFrom == IDC_LIST) {
            if (hdr->code == NM_CUSTOMDRAW) {
                return on_list_custom_draw((LPNMLVCUSTOMDRAW)lParam);
            }
            if (hdr->code == LVN_ITEMCHANGING) {
                if (!g_programmatic_select && !g_updating_list) {
                    LPNMLISTVIEW lv = (LPNMLISTVIEW)lParam;
                    if ((lv->uChanged & LVIF_STATE) && ((lv->uNewState ^ lv->uOldState) & LVIS_SELECTED)) {
                        return TRUE;
                    }
                }
                return 0;
            }
            if (hdr->code == LVN_KEYDOWN) {
                LPNMLVKEYDOWN key = (LPNMLVKEYDOWN)lParam;
                if (key->wVKey == VK_DELETE) {
                    delete_selected_task(hwnd);
                }
                return 0;
            }
            if (hdr->code == NM_DBLCLK) {
                LPNMITEMACTIVATE act = (LPNMITEMACTIVATE)lParam;
                LVHITTESTINFO ht;
                ht.pt = act->ptAction;
                int hit = ListView_HitTest(g_hwnd_list, &ht);

                if (hit >= 0 && !(ht.flags & LVHT_ONITEMSTATEICON)) {
                    select_list_item(hit);
                    begin_inline_edit(hit);
                }
                return 0;
            }
            if (hdr->code == NM_CLICK) {
                LPNMITEMACTIVATE click = (LPNMITEMACTIVATE)lParam;
                LVHITTESTINFO ht;
                ht.pt = click->ptAction;
                int hit = ListView_HitTest(g_hwnd_list, &ht);

                if (hit >= 0) {
                    if (ht.flags & LVHT_ONITEMSTATEICON) {
                        return 0;
                    }
                    if (ht.flags & (LVHT_ONITEMLABEL | LVHT_ONITEM)) {
                        toggle_list_item_selection(hit);
                    }
                } else {
                    clear_list_selection();
                }
                return 0;
            }
            if (hdr->code == LVN_ITEMCHANGED) {
                LPNMLISTVIEW lv = (LPNMLISTVIEW)lParam;
                if (g_updating_list) {
                    return 0;
                }
                if ((lv->uChanged & LVIF_STATE) && ((lv->uOldState ^ lv->uNewState) & LVIS_STATEIMAGEMASK)) {
                    int index = lv->iItem;
                    if (index >= 0 && index < g_todos.count) {
                        bool checked = ListView_GetCheckState(g_hwnd_list, index);
                        if (g_todos.items[index].done != checked) {
                            g_todos.items[index].done = checked;
                            ListView_RedrawItems(g_hwnd_list, index, index);
                            save_todos();
                            InvalidateRect(hwnd, NULL, FALSE);
                        }
                    }
                    sync_selection_visual();
                }
            }
        }
        return 0;
    }

    case WM_DESTROY:
        if (g_study_running) {
            stop_study(hwnd);
        }
        save_todos();
        save_study_log();
        todo_list_free(&g_todos);
        study_log_free(&g_study_log);
        destroy_fonts();
        if (g_brush_bg) DeleteObject(g_brush_bg);
        if (g_brush_surface) DeleteObject(g_brush_surface);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"TodoListWindow";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    if (!wc.hIcon) {
        wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Failed to register window class.", L"Todo List", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0, L"TodoListWindow", L"Todo List",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create window.", L"Todo List", MB_OK | MB_ICONERROR);
        return 1;
    }

    HMENU menu = CreateMenu();
    HMENU task_menu = CreateMenu();
    AppendMenuW(task_menu, MF_STRING, IDM_ADD, L"Add Task\tEnter");
    AppendMenuW(task_menu, MF_STRING, IDM_DELETE, L"Delete Selected\tDel");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)task_menu, L"Task");
    SetMenu(hwnd, menu);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            if (GetFocus() == g_hwnd_input) {
                add_task_from_input(hwnd);
                continue;
            }
            if (GetFocus() == g_hwnd_study_input && !g_study_running) {
                start_study(hwnd);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
