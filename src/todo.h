#ifndef TODO_H
#define TODO_H

#include <stdbool.h>
#include <wchar.h>

#define TODO_MAX_TEXT 256
#define TODO_MAX_DEADLINE 11

typedef struct {
    char text[TODO_MAX_TEXT];         /* UTF-8 */
    char deadline[TODO_MAX_DEADLINE]; /* "YYYY-MM-DD" or empty */
    bool done;
} TodoItem;

typedef struct {
    TodoItem *items;
    int count;
    int capacity;
} TodoList;

void todo_list_init(TodoList *list);
void todo_list_free(TodoList *list);

bool todo_get_data_path(wchar_t *buf, int bufsize);
bool todo_load(TodoList *list, const wchar_t *path);
bool todo_save(const TodoList *list, const wchar_t *path);

bool todo_add(TodoList *list, const char *text, const char *deadline);
bool todo_set_text(TodoList *list, int index, const char *text);
bool todo_remove(TodoList *list, int index);

bool todo_deadline_is_valid(const char *deadline);
bool todo_deadline_is_overdue(const char *deadline);
int todo_count_pending(const TodoList *list);

#endif
