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

#define IDC_INPUT       1001
#define IDC_ADD         1002
#define IDC_LIST        1003
#define IDC_DELETE      1004

#define IDM_ADD         2001
#define IDM_DELETE      2002

#define MARGIN          10
#define INPUT_HEIGHT    28
#define BUTTON_WIDTH    80
#define BUTTON_HEIGHT   28
#define BOTTOM_HEIGHT   36

static HWND g_hwnd_input;
static HWND g_hwnd_list;
static TodoList g_todos;
static char g_data_path[MAX_PATH];
static bool g_updating_list;
static HFONT g_font_normal;

static void layout_controls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int input_width = width - MARGIN * 3 - BUTTON_WIDTH;
    int list_height = height - MARGIN * 3 - INPUT_HEIGHT - BOTTOM_HEIGHT;

    if (input_width < 100) {
        input_width = 100;
    }
    if (list_height < 50) {
        list_height = 50;
    }

    SetWindowPos(g_hwnd_input, NULL, MARGIN, MARGIN, input_width, INPUT_HEIGHT, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, IDC_ADD), NULL, MARGIN * 2 + input_width, MARGIN, BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
    SetWindowPos(g_hwnd_list, NULL, MARGIN, MARGIN + INPUT_HEIGHT + MARGIN, width - MARGIN * 2, list_height, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, IDC_DELETE), NULL, MARGIN, height - MARGIN - BUTTON_HEIGHT, BUTTON_WIDTH + 40, BUTTON_HEIGHT, SWP_NOZORDER);
}

static void select_list_item(int index) {
    if (index < 0 || index >= g_todos.count) {
        return;
    }
    ListView_SetItemState(g_hwnd_list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

static void refresh_list_item(int index) {
    if (index < 0 || index >= g_todos.count) {
        return;
    }

    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_STATE;
    item.iItem = index;
    item.stateMask = LVIS_CUT;

    if (g_todos.items[index].done) {
        item.state = LVIS_CUT;
    } else {
        item.state = 0;
    }

    SendMessageW(g_hwnd_list, LVM_SETITEMW, 0, (LPARAM)&item);
    ListView_SetCheckState(g_hwnd_list, index, g_todos.items[index].done);
    select_list_item(index);
}

static int get_target_list_index(void) {
    int index = ListView_GetNextItem(g_hwnd_list, -1, LVNI_SELECTED);
    if (index >= 0) {
        return index;
    }
    return ListView_GetNextItem(g_hwnd_list, -1, LVNI_FOCUSED);
}

static void refresh_list(void) {
    g_updating_list = true;
    ListView_DeleteAllItems(g_hwnd_list);

    for (int i = 0; i < g_todos.count; i++) {
        wchar_t wtext[TODO_MAX_TEXT];
        MultiByteToWideChar(CP_ACP, 0, g_todos.items[i].text, -1, wtext, TODO_MAX_TEXT);

        LVITEMW item;
        memset(&item, 0, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_STATE;
        item.iItem = i;
        item.pszText = wtext;
        item.stateMask = LVIS_CUT;
        item.state = g_todos.items[i].done ? LVIS_CUT : 0;
        SendMessageW(g_hwnd_list, LVM_INSERTITEMW, 0, (LPARAM)&item);
        ListView_SetCheckState(g_hwnd_list, i, g_todos.items[i].done);
    }

    g_updating_list = false;
}

static bool save_todos(void) {
    return todo_save(&g_todos, g_data_path);
}

static void add_task_from_input(HWND hwnd) {
    char text[TODO_MAX_TEXT];
    GetWindowTextA(g_hwnd_input, text, sizeof(text));

    if (!todo_add(&g_todos, text)) {
        return;
    }

    SetWindowTextA(g_hwnd_input, "");
    refresh_list();
    save_todos();
    SetFocus(g_hwnd_input);
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

    SetFocus(g_hwnd_list);
    (void)hwnd;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd_input = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_INPUT, NULL, NULL);

        CreateWindowExA(
            0, "BUTTON", "Add",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_ADD, NULL, NULL);

        g_hwnd_list = CreateWindowExA(
            WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd, (HMENU)IDC_LIST, NULL, NULL);

        ListView_SetExtendedListViewStyle(g_hwnd_list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

        LVCOLUMNW col;
        memset(&col, 0, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = L"Task";
        col.cx = 400;
        SendMessageW(g_hwnd_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);

        CreateWindowExA(
            0, "BUTTON", "Delete Selected",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_DELETE, NULL, NULL);

        g_font_normal = CreateFontW(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        SendMessage(g_hwnd_input, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
        SendMessage(g_hwnd_list, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
        SendMessage(GetDlgItem(hwnd, IDC_ADD), WM_SETFONT, (WPARAM)g_font_normal, TRUE);
        SendMessage(GetDlgItem(hwnd, IDC_DELETE), WM_SETFONT, (WPARAM)g_font_normal, TRUE);

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
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
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
                            refresh_list_item(index);
                            save_todos();
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
        if (g_font_normal) {
            DeleteObject(g_font_normal);
        }
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
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "TodoListWindow";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "Todo List", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExA(
        0, "TodoListWindow", "Todo List",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 400,
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
