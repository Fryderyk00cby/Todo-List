#include "todo.h"

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

#define IDC_INPUT       1001
#define IDC_ADD         1002
#define IDC_LIST        1003
#define IDC_DELETE      1004
#define IDC_DUE_CHECK   1005
#define IDC_DUE_DATE    1006
#define IDC_EDIT_BOX    1007

#define IDM_ADD         2001
#define IDM_DELETE      2002

#define IDI_APPICON     101

/* Base layout metrics at 96 DPI; always go through scale(). */
#define MARGIN          14
#define GAP             8
#define HEADER_HEIGHT   50
#define INPUT_HEIGHT    38
#define BUTTON_WIDTH    84
#define BUTTON_HEIGHT   38
#define INPUT_MAX_WIDTH 260
#define DUE_CHECK_WIDTH 58
#define DUE_DATE_WIDTH  130
#define GAP_DUE_TO_DATE 12
#define BOTTOM_HEIGHT   48
#define DUE_COL_WIDTH   120
#define WINDOW_WIDTH    600
#define WINDOW_HEIGHT   540

#define CLR_BG          RGB(245, 246, 248)
#define CLR_SURFACE     RGB(255, 255, 255)
#define CLR_ACCENT      RGB(0, 120, 212)
#define CLR_TITLE       RGB(32, 32, 32)
#define CLR_MUTED       RGB(110, 110, 110)
#define CLR_DONE        RGB(140, 140, 140)
#define CLR_OVERDUE     RGB(196, 43, 28)
#define CLR_SELECT_BG   RGB(232, 244, 253)
#define CLR_SELECT_FG   RGB(32, 32, 32)

static HWND g_hwnd_input;
static HWND g_hwnd_list;
static HWND g_hwnd_due_check;
static HWND g_hwnd_due_date;
static HWND g_hwnd_edit_box;
static TodoList g_todos;
static wchar_t g_data_path[MAX_PATH];
static bool g_updating_list;
static bool g_programmatic_select;
static int g_selected_index;
static int g_editing_index = -1;
static int g_dpi = 96;
static HFONT g_font_title;
static HFONT g_font_normal;
static HFONT g_font_strike;
static HBRUSH g_brush_bg;
static HBRUSH g_brush_surface;

static void layout_controls(HWND hwnd);
static void refresh_list(void);
static void add_task_from_input(HWND hwnd);
static void delete_selected_task(HWND hwnd);
static void update_due_date_enabled(void);
static void get_deadline_from_ui(char *buf, int bufsize);
static void end_inline_edit(bool commit);

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

    /* If the UTF-8 form doesn't fit, trim trailing characters until it does. */
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
}

static void destroy_fonts(void) {
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_normal) DeleteObject(g_font_normal);
    if (g_font_strike) DeleteObject(g_font_strike);
    g_font_title = NULL;
    g_font_normal = NULL;
    g_font_strike = NULL;
}

static void apply_fonts(HWND hwnd) {
    HWND controls[] = {
        g_hwnd_input, g_hwnd_due_check, g_hwnd_due_date,
        GetDlgItem(hwnd, IDC_ADD), g_hwnd_list, GetDlgItem(hwnd, IDC_DELETE),
        g_hwnd_edit_box
    };
    for (int i = 0; i < (int)(sizeof(controls) / sizeof(controls[0])); i++) {
        SendMessageW(controls[i], WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    }
}

static void layout_controls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    end_inline_edit(true);

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int top = scale(HEADER_HEIGHT + MARGIN);
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
    SetWindowPos(g_hwnd_input, NULL, x, top, input_width, scale(INPUT_HEIGHT), SWP_NOZORDER);
    x += input_width + scale(GAP);

    SetWindowPos(g_hwnd_due_check, NULL, x, top + scale(8), scale(DUE_CHECK_WIDTH), scale(INPUT_HEIGHT), SWP_NOZORDER);
    x += scale(DUE_CHECK_WIDTH + GAP_DUE_TO_DATE);

    SetWindowPos(g_hwnd_due_date, NULL, x, top + scale(4), scale(DUE_DATE_WIDTH), scale(INPUT_HEIGHT), SWP_NOZORDER);
    x += scale(DUE_DATE_WIDTH + GAP);

    SetWindowPos(GetDlgItem(hwnd, IDC_ADD), NULL, x, top, scale(BUTTON_WIDTH), scale(BUTTON_HEIGHT), SWP_NOZORDER);
    SetWindowPos(g_hwnd_list, NULL, scale(MARGIN), list_top, width - scale(MARGIN) * 2, list_height, SWP_NOZORDER);

    int delete_width = scale(130);
    SetWindowPos(
        GetDlgItem(hwnd, IDC_DELETE), NULL,
        width - scale(MARGIN) - delete_width, height - scale(MARGIN) - scale(BUTTON_HEIGHT),
        delete_width, scale(BUTTON_HEIGHT), SWP_NOZORDER);

    if (g_hwnd_list) {
        int list_width = width - scale(MARGIN) * 2;
        ListView_SetColumnWidth(g_hwnd_list, 0, list_width - scale(DUE_COL_WIDTH + 10));
        ListView_SetColumnWidth(g_hwnd_list, 1, scale(DUE_COL_WIDTH));
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

static void paint_header(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

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

    RECT title_rc = header;
    title_rc.left = scale(MARGIN);
    title_rc.right = rc.right / 2;
    DrawTextW(hdc, L"Todo List", -1, &title_rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    wchar_t stats[64];
    int pending = todo_count_pending(&g_todos);
    wsprintfW(stats, L"%d pending", pending);
    SelectObject(hdc, g_font_normal);
    SetTextColor(hdc, CLR_MUTED);

    RECT stats_rc = header;
    stats_rc.right = rc.right - scale(MARGIN);
    DrawTextW(hdc, stats, -1, &stats_rc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

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

        apply_fonts(hwnd);

        SYSTEMTIME today;
        GetLocalTime(&today);
        SendMessageW(g_hwnd_due_date, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&today);
        EnableWindow(g_hwnd_due_date, FALSE);

        todo_list_init(&g_todos);
        if (!todo_get_data_path(g_data_path, MAX_PATH)) {
            wcscpy(g_data_path, L"todos.dat");
        }
        g_selected_index = -1;
        g_programmatic_select = false;

        todo_load(&g_todos, g_data_path);
        refresh_list();

        SetWindowPos(hwnd, NULL, 0, 0, scale(WINDOW_WIDTH), scale(WINDOW_HEIGHT),
            SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
        layout_controls(hwnd);
        SetFocus(g_hwnd_input);
        return 0;
    }

    case WM_SIZE:
        layout_controls(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
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

    case WM_PAINT:
        paint_header(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
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
        }
        break;

    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lParam;
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
        save_todos();
        todo_list_free(&g_todos);
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
        WS_OVERLAPPEDWINDOW,
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
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && GetFocus() == g_hwnd_input) {
            add_task_from_input(hwnd);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
