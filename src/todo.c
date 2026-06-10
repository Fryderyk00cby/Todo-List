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

static void todo_sanitize_text(char *text) {
    char *newline = strchr(text, '\n');
    if (newline) {
        *newline = '\0';
    }
    newline = strchr(text, '\r');
    if (newline) {
        *newline = '\0';
    }
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
        item->done = line[0] == '1';
        strncpy(item->text, line + 2, TODO_MAX_TEXT - 1);
        item->text[TODO_MAX_TEXT - 1] = '\0';
        todo_sanitize_text(item->text);
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
        fprintf(fp, "%d|%s\n", list->items[i].done ? 1 : 0, list->items[i].text);
    }

    fclose(fp);
    return true;
}

bool todo_add(TodoList *list, const char *text) {
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
