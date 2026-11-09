/* Build/run only inside .ui-check/reset-test; never uses the user's data directory. */
#define WinMain unused_app_entry
#include "../src/main.c"
#undef WinMain
#include <assert.h>

static wchar_t test_directory[MAX_PATH];
static const wchar_t *test_names[] = {L"todos.dat", L"study.dat", L"appearance.ini",
    L"settings.ini", L"todos.dat.tmp", L"study.dat.tmp", L"memos.dat", L"memos.dat.tmp"};

static void path_for(const wchar_t *name, wchar_t *path) {
    wsprintfW(path, L"%s\\%s", test_directory, name);
}

static void write_fixture(const wchar_t *name, const char *data) {
    wchar_t path[MAX_PATH];
    path_for(name, path);
    FILE *file = _wfopen(path, L"wb");
    assert(file);
    assert(fputs(data, file) >= 0);
    assert(fclose(file) == 0);
}

static void expect_contents(const wchar_t *name, const char *expected) {
    wchar_t path[MAX_PATH];
    path_for(name, path);
    FILE *file = _wfopen(path, L"rb");
    char data[256] = {0};
    assert(file);
    size_t length = fread(data, 1, sizeof(data) - 1, file);
    assert(!ferror(file));
    assert(fclose(file) == 0);
    assert(length == strlen(expected) && strcmp(data, expected) == 0);
}

static void expect_absent(void) {
    for (int i = 0; i < 8; ++i) {
        wchar_t path[MAX_PATH], backup[MAX_PATH];
        path_for(test_names[i], path);
        assert(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
        wsprintfW(backup, L"%s.reset-backup", path);
        assert(GetFileAttributesW(backup) == INVALID_FILE_ATTRIBUTES);
    }
}

int main(void) {
    GetModuleFileNameW(NULL, test_directory, MAX_PATH);
    wchar_t *slash = wcsrchr(test_directory, L'\\');
    assert(slash);
    *slash = 0;
    assert(wcsstr(test_directory, L"\\.ui-check\\reset-test") != NULL);
    DWORD error;
    assert(factory_reset_files(test_directory, &error) == RESET_OK);
    write_fixture(L"unrelated.dat", "must stay");
    for (int i = 0; i < 8; ++i) write_fixture(test_names[i], "fixture");

    wchar_t locked_path[MAX_PATH];
    path_for(L"study.dat", locked_path);
    HANDLE locked = CreateFileW(locked_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    assert(locked != INVALID_HANDLE_VALUE);
    assert(factory_reset_files(test_directory, &error) == RESET_FAILED);
    for (int i = 0; i < 8; ++i) expect_contents(test_names[i], "fixture");
    CloseHandle(locked);
    assert(SetFileAttributesW(locked_path, FILE_ATTRIBUTE_READONLY));
    assert(factory_reset_files(test_directory, &error) == RESET_FAILED);
    for (int i = 0; i < 8; ++i) expect_contents(test_names[i], "fixture");
    assert(SetFileAttributesW(locked_path, FILE_ATTRIBUTE_NORMAL));

    write_fixture(L"study.dat.reset-backup", "recovery file");
    assert(factory_reset_files(test_directory, &error) == RESET_FAILED);
    expect_contents(L"study.dat.reset-backup", "recovery file");
    wchar_t backup[MAX_PATH];
    path_for(L"study.dat.reset-backup", backup);
    assert(DeleteFileW(backup));
    assert(factory_reset_files(test_directory, &error) == RESET_OK);
    expect_absent();
    expect_contents(L"unrelated.dat", "must stay");
    assert(factory_reset_files(test_directory, &error) == RESET_OK);
    puts("PASS: reset, missing files, unrelated data, read-only files, locked-file rollback, recovery-file protection");

    write_fixture(L"todos.dat", "0||Task to reset\n");
    write_fixture(L"study.dat", "2026-11-08|09:00:00|10:00:00|3600|Focus to reset\n");
    write_fixture(L"memos.dat", "2024-02-29|Memo to reset\n");
    write_fixture(L"appearance.ini", "[Appearance]\nTheme=2\n");
    write_fixture(L"settings.ini", "[General]\nLanguage=zh-CN\n");
    language_load();
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSW wc = {0};
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"ResetIntegrationTest";
    assert(RegisterClassW(&wc));
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Reset test", WS_OVERLAPPEDWINDOW,
        0, 0, 800, 720, NULL, NULL, wc.hInstance, NULL);
    assert(hwnd);
    assert(g_todos.count == 1 && g_study_log.count == 1);
    assert(app_theme_index == 2 && app_language == LANGUAGE_CHINESE);
    switch_view(hwnd, VIEW_MEMO);
    assert(g_view == VIEW_MEMO);
    HWND memo_editor = GetDlgItem(g_hwnd_memo, 1);
    assert(memo_editor);
    SendMessageW(memo_editor, EM_REPLACESEL, TRUE, (LPARAM)L"Memo integration check");
    switch_view(hwnd, VIEW_TODO);
    wchar_t memo_path[MAX_PATH];
    path_for(L"memos.dat", memo_path);
    MemoBook memos = {0};
    assert(memo_book_load(&memos, memo_path) && memos.count == 2);
    memo_book_free(&memos);
    SetWindowTextW(g_hwnd_study_input, L"Unfinished focus session");
    start_study(hwnd);
    g_study_start_tick -= 65000;
    SetWindowTextW(g_hwnd_input, L"Unsaved task");
    switch_view(hwnd, VIEW_SETTINGS);

    locked = CreateFileW(locked_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    assert(locked != INVALID_HANDLE_VALUE);
    assert(reset_application(hwnd, &error) == RESET_FAILED);
    assert(g_todos.count == 1 && g_study_log.count == 1 && g_study_running);
    assert(app_theme_index == 2 && app_language == LANGUAGE_CHINESE);
    CloseHandle(locked);
    assert(reset_application(hwnd, &error) == RESET_OK);
    assert(GetWindowTextLengthW(memo_editor) == 0);
    assert(g_todos.count == 0 && g_study_log.count == 0 && !g_study_running);
    assert(app_theme_index == 0 && app_language == LANGUAGE_ENGLISH);
    assert(GetWindowTextLengthW(g_hwnd_input) == 0 && GetWindowTextLengthW(g_hwnd_study_input) == 0);
    assert(ListView_GetItemCount(g_hwnd_list) == 0 && ListView_GetItemCount(g_hwnd_hist_list) == 0);
    expect_absent();
    DestroyWindow(hwnd);
    expect_contents(L"todos.dat", "");
    expect_contents(L"study.dat", "");
    appearance_load();
    language_load();
    assert(app_theme_index == 0 && app_language == LANGUAGE_ENGLISH);
    TodoList tasks;
    StudyLog sessions;
    todo_list_init(&tasks);
    study_log_init(&sessions);
    assert(todo_load(&tasks, g_data_path) && study_load(&sessions, g_study_path));
    assert(tasks.count == 0 && sessions.count == 0);
    todo_list_free(&tasks);
    study_log_free(&sessions);
    puts("PASS: active session discard, state reset, failure preservation, defaults, shutdown/reload without old records");
    return 0;
}
