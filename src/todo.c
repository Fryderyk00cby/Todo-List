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
    if (len != 10) {
        return false;
    }
    return value[4] == '-' && value[7] == '-'
        && value[0] >= '0' && value[0] <= '9'
        && value[9] >= '0' && value[9] <= '9';
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

bool todo_get_data_path(char *buf, int bufsize) {
    char exe_path[MAX_PATH];

    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
        return false;
    }

    char *slash = strrchr(exe_path, '\\');
    if (!slash) {
        return false;
    }
    *(slash + 1) = '\0';

    if ((int)strlen(exe_path) + 9 >= bufsize) {
        return false;
    }

    strcpy(buf, exe_path);
    strcat(buf, "todos.dat");
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
    sprintf(today, "%04d-%02d-%02d", now.wYear, now.wMonth, now.wDay);
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
}

bool todo_load(TodoList *list, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return true;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (len < 3 || (line[0] != '0' && line[0] != '1') || line[1] != '|') {
            continue;
        }

        if (list->count >= list->capacity && !todo_grow(list)) {
            fclose(fp);
            return false;
        }

        TodoItem *item = &list->items[list->count++];
        todo_parse_line(line, &item->done, item->deadline, item->text);
    }

    fclose(fp);
    return true;
}

bool todo_save(const TodoList *list, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return false;
    }

    for (int i = 0; i < list->count; i++) {
        fprintf(fp, "%d|%s|%s\n",
            list->items[i].done ? 1 : 0,
            list->items[i].deadline,
            list->items[i].text);
    }

    fclose(fp);
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
    if (deadline && deadline[0] != '\0' && todo_deadline_is_valid(deadline)) {
        strncpy(item->deadline, deadline, TODO_MAX_DEADLINE - 1);
        item->deadline[TODO_MAX_DEADLINE - 1] = '\0';
    }

    item->done = false;
    return item->text[0] != '\0';
}

bool todo_toggle(TodoList *list, int index) {
    if (index < 0 || index >= list->count) {
        return false;
    }
    list->items[index].done = !list->items[index].done;
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
