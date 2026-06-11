#include "todo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TODO_INITIAL_CAPACITY 16

static bool todo_grow(TodoList *list) {
    int new_cap = list->capacity == 0 ? TODO_INITIAL_CAPACITY : list->capacity * 2;
    TodoItem *new_items = (TodoItem *)realloc(list->items, (size_t)new_cap * sizeof(TodoItem));
    if (!new_items) {
        return false;
    }
    list->items = new_items;
    list->capacity = new_cap;
    return true;
}

static void todo_clear_deadline(char *deadline) {
    deadline[0] = '\0';
}

static bool todo_looks_like_date(const char *value, size_t len) {
    static const int digit_pos[] = {0, 1, 2, 3, 5, 6, 8, 9};

    if (len != 10 || value[4] != '-' || value[7] != '-') {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        char c = value[digit_pos[i]];
        if (c < '0' || c > '9') {
            return false;
        }
    }

    int month = (value[5] - '0') * 10 + (value[6] - '0');
    int day = (value[8] - '0') * 10 + (value[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

static void todo_sanitize_text(char *text) {
    char *newline = strchr(text, '\n');
    if (newline) {
        *newline = '\0';
    }
    newline = strchr(text, '\r');
    if (newline) {
        *newline = '\0';
    }

    for (char *p = text; *p; p++) {
        if (*p == '|') {
            *p = ' ';
        }
    }
}

static bool todo_is_valid_utf8(const char *s) {
    const unsigned char *p = (const unsigned char *)s;

    while (*p) {
        int extra;
        if (*p < 0x80) {
            p++;
            continue;
        }
        if ((*p & 0xE0) == 0xC0) {
            extra = 1;
        } else if ((*p & 0xF0) == 0xE0) {
            extra = 2;
        } else if ((*p & 0xF8) == 0xF0) {
            extra = 3;
        } else {
            return false;
        }
        p++;
        for (int i = 0; i < extra; i++, p++) {
            if ((*p & 0xC0) != 0x80) {
                return false;
            }
        }
    }
    return true;
}

/* Best-effort in-place conversion of legacy ANSI text to UTF-8. */
static void todo_ansi_to_utf8(char *text) {
    wchar_t wide[TODO_MAX_TEXT];
    char utf8[TODO_MAX_TEXT];

    if (!MultiByteToWideChar(CP_ACP, 0, text, -1, wide, TODO_MAX_TEXT)) {
        return;
    }
    if (!WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), NULL, NULL)) {
        return;
    }
    strcpy(text, utf8);
}

void todo_list_init(TodoList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void todo_list_free(TodoList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

bool todo_get_data_path(wchar_t *buf, int bufsize) {
    wchar_t exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);

    if (len == 0 || len >= MAX_PATH) {
        return false;
    }

    wchar_t *slash = wcsrchr(exe_path, L'\\');
    if (!slash) {
        return false;
    }
    *(slash + 1) = L'\0';

    if ((int)(wcslen(exe_path) + wcslen(L"todos.dat")) >= bufsize) {
        return false;
    }

    wcscpy(buf, exe_path);
    wcscat(buf, L"todos.dat");
    return true;
}

bool todo_deadline_is_valid(const char *deadline) {
    if (!deadline || deadline[0] == '\0') {
        return false;
    }
    return todo_looks_like_date(deadline, strlen(deadline));
}

bool todo_deadline_is_overdue(const char *deadline) {
    if (!todo_deadline_is_valid(deadline)) {
        return false;
    }

    SYSTEMTIME now;
    char today[TODO_MAX_DEADLINE];

    GetLocalTime(&now);
    snprintf(today, sizeof(today), "%04d-%02d-%02d", now.wYear, now.wMonth, now.wDay);
    return strcmp(deadline, today) < 0;
}

int todo_count_pending(const TodoList *list) {
    int pending = 0;
    for (int i = 0; i < list->count; i++) {
        if (!list->items[i].done) {
            pending++;
        }
    }
    return pending;
}

static void todo_parse_line(const char *line, bool *done, char *deadline, char *text) {
    *done = line[0] == '1';
    todo_clear_deadline(deadline);
    text[0] = '\0';

    const char *rest = line + 2;
    const char *second_pipe = strchr(rest, '|');

    if (second_pipe && (second_pipe == rest || todo_looks_like_date(rest, (size_t)(second_pipe - rest)))) {
        size_t deadline_len = (size_t)(second_pipe - rest);
        if (deadline_len > 0) {
            strncpy(deadline, rest, TODO_MAX_DEADLINE - 1);
            deadline[TODO_MAX_DEADLINE - 1] = '\0';
        }
        strncpy(text, second_pipe + 1, TODO_MAX_TEXT - 1);
    } else {
        strncpy(text, rest, TODO_MAX_TEXT - 1);
    }

    text[TODO_MAX_TEXT - 1] = '\0';
    todo_sanitize_text(text);

    /* Legacy files written before the UTF-8 switch used the ANSI codepage. */
    if (!todo_is_valid_utf8(text)) {
        todo_ansi_to_utf8(text);
    }
}

bool todo_load(TodoList *list, const wchar_t *path) {
    FILE *fp = _wfopen(path, L"r");
    if (!fp) {
        return true;
    }

    char line[1024];
    bool first_line = true;

    while (fgets(line, sizeof(line), fp)) {
        char *start = line;

        /* Skip a UTF-8 BOM, e.g. when the file was edited in Notepad. */
        if (first_line && (unsigned char)start[0] == 0xEF
                && (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF) {
            start += 3;
        }
        first_line = false;

        size_t len = strlen(start);
        while (len > 0 && (start[len - 1] == '\n' || start[len - 1] == '\r')) {
            start[--len] = '\0';
        }

        if (len < 3 || (start[0] != '0' && start[0] != '1') || start[1] != '|') {
            continue;
        }

        if (list->count >= list->capacity && !todo_grow(list)) {
            fclose(fp);
            return false;
        }

        TodoItem *item = &list->items[list->count++];
        todo_parse_line(start, &item->done, item->deadline, item->text);
    }

    fclose(fp);
    return true;
}

bool todo_save(const TodoList *list, const wchar_t *path) {
    wchar_t tmp_path[MAX_PATH + 8];

    if (wcslen(path) + 5 >= sizeof(tmp_path) / sizeof(tmp_path[0])) {
        return false;
    }
    wcscpy(tmp_path, path);
    wcscat(tmp_path, L".tmp");

    FILE *fp = _wfopen(tmp_path, L"w");
    if (!fp) {
        return false;
    }

    bool ok = true;
    for (int i = 0; i < list->count; i++) {
        if (fprintf(fp, "%d|%s|%s\n",
                list->items[i].done ? 1 : 0,
                list->items[i].deadline,
                list->items[i].text) < 0) {
            ok = false;
            break;
        }
    }

    if (fclose(fp) != 0) {
        ok = false;
    }

    if (!ok || !MoveFileExW(tmp_path, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp_path);
        return false;
    }
    return true;
}

bool todo_add(TodoList *list, const char *text, const char *deadline) {
    if (!text || text[0] == '\0') {
        return false;
    }

    if (list->count >= list->capacity && !todo_grow(list)) {
        return false;
    }

    TodoItem *item = &list->items[list->count++];
    strncpy(item->text, text, TODO_MAX_TEXT - 1);
    item->text[TODO_MAX_TEXT - 1] = '\0';
    todo_sanitize_text(item->text);

    todo_clear_deadline(item->deadline);
    if (deadline && todo_deadline_is_valid(deadline)) {
        strncpy(item->deadline, deadline, TODO_MAX_DEADLINE - 1);
        item->deadline[TODO_MAX_DEADLINE - 1] = '\0';
    }

    item->done = false;

    if (item->text[0] == '\0') {
        list->count--;
        return false;
    }
    return true;
}

bool todo_set_text(TodoList *list, int index, const char *text) {
    if (index < 0 || index >= list->count || !text || text[0] == '\0') {
        return false;
    }

    char sanitized[TODO_MAX_TEXT];
    strncpy(sanitized, text, TODO_MAX_TEXT - 1);
    sanitized[TODO_MAX_TEXT - 1] = '\0';
    todo_sanitize_text(sanitized);

    if (sanitized[0] == '\0') {
        return false;
    }

    strcpy(list->items[index].text, sanitized);
    return true;
}

bool todo_remove(TodoList *list, int index) {
    if (index < 0 || index >= list->count) {
        return false;
    }

    for (int i = index; i < list->count - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    list->count--;
    return true;
}
