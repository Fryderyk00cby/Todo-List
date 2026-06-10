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
#ifndef LVM_SETITEMHEIGHT
#define LVM_SETITEMHEIGHT (LVM_FIRST + 51)
#endif

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define IDC_INPUT       1001
#define IDC_ADD         1002
#define IDC_LIST        1003
#define IDC_DELETE      1004
#define IDC_DUE_CHECK   1005
#define IDC_DUE_DATE    1006

#define IDM_ADD         2001
#define IDM_DELETE      2002

#define MARGIN          14
#define GAP             8
#define HEADER_HEIGHT   42
#define INPUT_HEIGHT    32
#define BUTTON_WIDTH    76
#define BUTTON_HEIGHT   32
#define DUE_CHECK_WIDTH 44
#define DUE_DATE_WIDTH  118
#define BOTTOM_HEIGHT   44
#define LIST_ROW_HEIGHT 30

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
static TodoList g_todos;
static char g_data_path[MAX_PATH];
static bool g_updating_list;
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

static void layout_controls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int top = HEADER_HEIGHT + MARGIN;
    int right = width - MARGIN;
    int fixed_right = BUTTON_WIDTH + GAP + DUE_DATE_WIDTH + GAP + DUE_CHECK_WIDTH;
    int input_width = right - MARGIN - fixed_right;

    if (input_width < 120) {
        input_width = 120;
    }

    int list_top = top + INPUT_HEIGHT + GAP;
    int list_height = height - list_top - BOTTOM_HEIGHT - MARGIN;

    if (list_height < 80) {
        list_height = 80;
    }

    int x = MARGIN;
    SetWindowPos(g_hwnd_input, NULL, x, top, input_width, INPUT_HEIGHT, SWP_NOZORDER);
    x += input_width + GAP;

    SetWindowPos(g_hwnd_due_check, NULL, x, top + 6, DUE_CHECK_WIDTH, INPUT_HEIGHT, SWP_NOZORDER);
    x += DUE_CHECK_WIDTH + GAP;

    SetWindowPos(g_hwnd_due_date, NULL, x, top + 2, DUE_DATE_WIDTH, INPUT_HEIGHT, SWP_NOZORDER);
    x += DUE_DATE_WIDTH + GAP;

    SetWindowPos(GetDlgItem(hwnd, IDC_ADD), NULL, x, top, BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
    SetWindowPos(g_hwnd_list, NULL, MARGIN, list_top, width - MARGIN * 2, list_height, SWP_NOZORDER);

    int delete_width = 130;
    SetWindowPos(
        GetDlgItem(hwnd, IDC_DELETE), NULL,
        width - MARGIN - delete_width, height - MARGIN - BUTTON_HEIGHT,
        delete_width, BUTTON_HEIGHT, SWP_NOZORDER);

    if (g_hwnd_list) {
        int list_width = width - MARGIN * 2;
        ListView_SetColumnWidth(g_hwnd_list, 0, list_width - 130);
        ListView_SetColumnWidth(g_hwnd_list, 1, 120);
    }
}

static void select_list_item(int index) {
    if (index < 0 || index >= g_todos.count) {
        return;
    }
    ListView_SetItemState(g_hwnd_list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
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
    g_updating_list = true;
    ListView_DeleteAllItems(g_hwnd_list);

    for (int i = 0; i < g_todos.count; i++) {
        wchar_t wtext[TODO_MAX_TEXT];
        wchar_t wdue[TODO_MAX_DEADLINE];
        char due_buf[TODO_MAX_DEADLINE];

        MultiByteToWideChar(CP_ACP, 0, g_todos.items[i].text, -1, wtext, TODO_MAX_TEXT);
        MultiByteToWideChar(CP_ACP, 0, deadline_display(&g_todos.items[i], due_buf, sizeof(due_buf)), -1, wdue, TODO_MAX_DEADLINE);

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
    InvalidateRect(GetParent(g_hwnd_list), NULL, FALSE);
}

static bool save_todos(void) {
    return todo_save(&g_todos, g_data_path);
}

static void update_due_date_enabled(void) {
    BOOL enabled = (SendMessage(g_hwnd_due_check, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(g_hwnd_due_date, enabled);
}

static void get_deadline_from_ui(char *buf, int bufsize) {
    buf[0] = '\0';
    if (SendMessage(g_hwnd_due_check, BM_GETCHECK, 0, 0) != BST_CHECKED) {
        return;
    }

    SYSTEMTIME st;
    if (SendMessage(g_hwnd_due_date, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) != GDT_VALID) {
        return;
    }

    sprintf(buf, "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    buf[bufsize - 1] = '\0';
}

static void add_task_from_input(HWND hwnd) {
    char text[TODO_MAX_TEXT];
    char deadline[TODO_MAX_DEADLINE];

    GetWindowTextA(g_hwnd_input, text, sizeof(text));
    get_deadline_from_ui(deadline, sizeof(deadline));

    if (!todo_add(&g_todos, text, deadline[0] ? deadline : NULL)) {
        return;
    }

    SetWindowTextA(g_hwnd_input, "");
    SendMessage(g_hwnd_due_check, BM_SETCHECK, BST_UNCHECKED, 0);
    update_due_date_enabled();
    refresh_list();
    save_todos();
    InvalidateRect(hwnd, NULL, FALSE);
    SetFocus(g_hwnd_input);
}

static int get_target_list_index(void) {
    int index = ListView_GetNextItem(g_hwnd_list, -1, LVNI_SELECTED);
    if (index >= 0) {
        return index;
    }
    return ListView_GetNextItem(g_hwnd_list, -1, LVNI_FOCUSED);
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
        ListView_SetItemState(g_hwnd_list, next, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }

    InvalidateRect(hwnd, NULL, FALSE);
    SetFocus(g_hwnd_list);
}

static void paint_header(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);

    RECT header = rc;
    header.bottom = HEADER_HEIGHT;
    FillRect(hdc, &header, g_brush_surface);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(230, 230, 230));
    HPEN old_pen = SelectObject(hdc, border);
    MoveToEx(hdc, 0, HEADER_HEIGHT - 1, NULL);
    LineTo(hdc, rc.right, HEADER_HEIGHT - 1);
    SelectObject(hdc, old_pen);
    DeleteObject(border);

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_font_title);
    SetTextColor(hdc, CLR_TITLE);

    RECT title_rc = header;
    title_rc.left = MARGIN;
    title_rc.right = rc.right / 2;
    DrawTextW(hdc, L"Todo List", -1, &title_rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    wchar_t stats[64];
    int pending = todo_count_pending(&g_todos);
    wsprintfW(stats, L"%d pending", pending);
    SelectObject(hdc, g_font_normal);
    SetTextColor(hdc, CLR_MUTED);

    RECT stats_rc = header;
    stats_rc.right = rc.right - MARGIN;
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
        bool selected = (ListView_GetItemState(g_hwnd_list, index, LVIS_SELECTED) & LVIS_SELECTED) != 0;
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
        g_brush_bg = CreateSolidBrush(CLR_BG);
        g_brush_surface = CreateSolidBrush(CLR_SURFACE);

        g_font_title = CreateFontW(
            -20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_font_normal = CreateFontW(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_font_strike = CreateFontW(
            -15, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hwnd_input = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_INPUT, NULL, NULL);

        g_hwnd_due_check = CreateWindowExA(
            0, "BUTTON", "Due",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_DUE_CHECK, NULL, NULL);

        g_hwnd_due_date = CreateWindowExA(
            0, DATETIMEPICK_CLASSA, "",
            WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT,
            0, 0, 0, 0, hwnd, (HMENU)IDC_DUE_DATE, NULL, NULL);

        CreateWindowExA(
            0, "BUTTON", "Add",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_ADD, NULL, NULL);

        g_hwnd_list = CreateWindowExA(
            WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd, (HMENU)IDC_LIST, NULL, NULL);

        ListView_SetExtendedListViewStyle(
            g_hwnd_list,
            LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

        SendMessage(g_hwnd_list, LVM_SETITEMHEIGHT, LIST_ROW_HEIGHT, 0);
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

        CreateWindowExA(
            0, "BUTTON", "Delete Selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_DELETE, NULL, NULL);

        HWND controls[] = {
            g_hwnd_input, g_hwnd_due_check, g_hwnd_due_date,
            GetDlgItem(hwnd, IDC_ADD), g_hwnd_list, GetDlgItem(hwnd, IDC_DELETE)
        };
        for (int i = 0; i < 6; i++) {
            SendMessage(controls[i], WM_SETFONT, (WPARAM)g_font_normal, TRUE);
        }

        SYSTEMTIME today;
        GetLocalTime(&today);
        SendMessage(g_hwnd_due_date, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&today);
        EnableWindow(g_hwnd_due_date, FALSE);

        todo_list_init(&g_todos);
        if (!todo_get_data_path(g_data_path, sizeof(g_data_path))) {
            strcpy(g_data_path, "todos.dat");
        }
        todo_load(&g_todos, g_data_path);
        refresh_list();
        layout_controls(hwnd);
        SetFocus(g_hwnd_input);
        return 0;
    }

    case WM_SIZE:
        layout_controls(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

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
            if (hdr->code == NM_CLICK) {
                LPNMITEMACTIVATE click = (LPNMITEMACTIVATE)lParam;
                if (click->iItem >= 0) {
                    select_list_item(click->iItem);
                }
                return 0;
            }
            if (hdr->code == LVN_ITEMCHANGED) {
                LPNMLISTVIEW lv = (LPNMLISTVIEW)lParam;
                if (g_updating_list) {
                    return 0;
                }
                if ((lv->uChanged & LVIF_STATE) && (lv->uNewState & LVIS_STATEIMAGEMASK)) {
                    int index = lv->iItem;
                    if (index >= 0 && index < g_todos.count) {
                        select_list_item(index);
                        bool checked = ListView_GetCheckState(g_hwnd_list, index);
                        if (g_todos.items[index].done != checked) {
                            g_todos.items[index].done = checked;
                            ListView_RedrawItems(g_hwnd_list, index, index);
                            save_todos();
                            InvalidateRect(hwnd, NULL, FALSE);
                        }
                    }
                }
            }
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_DELETE && GetFocus() == g_hwnd_list) {
            delete_selected_task(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        save_todos();
        todo_list_free(&g_todos);
        if (g_font_title) DeleteObject(g_font_title);
        if (g_font_normal) DeleteObject(g_font_normal);
        if (g_font_strike) DeleteObject(g_font_strike);
        if (g_brush_bg) DeleteObject(g_brush_bg);
        if (g_brush_surface) DeleteObject(g_brush_surface);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "TodoListWindow";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "Todo List", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExA(
        0, "TodoListWindow", "Todo List",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 500,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create window.", "Todo List", MB_OK | MB_ICONERROR);
        return 1;
    }

    HMENU menu = CreateMenu();
    HMENU task_menu = CreateMenu();
    AppendMenuA(task_menu, MF_STRING, IDM_ADD, "Add Task\tEnter");
    AppendMenuA(task_menu, MF_STRING, IDM_DELETE, "Delete Selected\tDel");
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)task_menu, "Task");
    SetMenu(hwnd, menu);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && GetFocus() == g_hwnd_input) {
            add_task_from_input(hwnd);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
