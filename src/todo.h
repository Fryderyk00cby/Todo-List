#ifndef TODO_H
#define TODO_H

#include <stdbool.h>

#define TODO_MAX_TEXT 256

typedef struct {
    char text[TODO_MAX_TEXT];
    bool done;
} TodoItem;

typedef struct {
    TodoItem *items;
    int count;
    int capacity;
} TodoList;

void todo_list_init(TodoList *list);
void todo_list_free(TodoList *list);

bool todo_get_data_path(char *buf, int bufsize);
bool todo_load(TodoList *list, const char *path);
bool todo_save(const TodoList *list, const char *path);

bool todo_add(TodoList *list, const char *text);
bool todo_toggle(TodoList *list, int index);
bool todo_remove(TodoList *list, int index);

#endif
