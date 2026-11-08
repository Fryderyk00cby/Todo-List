#include "factory_reset.h"
#include <wchar.h>

ResetResult factory_reset_files(const wchar_t *directory, DWORD *error) {
    static const wchar_t *names[] = {
        L"todos.dat", L"study.dat", L"appearance.ini", L"settings.ini",
        L"todos.dat.tmp", L"study.dat.tmp"
    };
    wchar_t paths[6][MAX_PATH], backups[6][MAX_PATH];
    BOOL exists[6] = {0}, moved[6] = {0};
    *error = ERROR_SUCCESS;
    /* Check every path before moving anything; never overwrite recovery files. */
    for (int i = 0; i < 6; ++i) {
        if (wcslen(directory) + wcslen(names[i]) + 16 >= MAX_PATH) {
            *error = ERROR_FILENAME_EXCED_RANGE;
            return RESET_FAILED;
        }
        wsprintfW(paths[i], L"%s\\%s", directory, names[i]);
        wsprintfW(backups[i], L"%s.reset-backup", paths[i]);
        DWORD attributes = GetFileAttributesW(backups[i]);
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            *error = ERROR_ALREADY_EXISTS;
            return RESET_FAILED;
        }
        if (GetLastError() != ERROR_FILE_NOT_FOUND) {
            *error = GetLastError();
            return RESET_FAILED;
        }
        attributes = GetFileAttributesW(paths[i]);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) continue;
            *error = GetLastError();
            return RESET_FAILED;
        }
        if (attributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
            *error = ERROR_ACCESS_DENIED;
            return RESET_FAILED;
        }
        exists[i] = TRUE;
    }
    /* Flush profile caches before moving preferences out of the active paths. */
    for (int i = 2; i <= 3; ++i)
        if (exists[i]) WritePrivateProfileStringW(NULL, NULL, NULL, paths[i]);
    for (int i = 0; i < 6; ++i) {
        if (!exists[i]) continue;
        if (!MoveFileExW(paths[i], backups[i], MOVEFILE_WRITE_THROUGH)) {
            *error = GetLastError();
            ResetResult result = RESET_FAILED;
            for (int j = i - 1; j >= 0; --j) {
                if (moved[j] && !MoveFileExW(backups[j], paths[j], MOVEFILE_WRITE_THROUGH))
                    result = RESET_RECOVERY_NEEDED;
            }
            return result;
        }
        moved[i] = TRUE;
    }
    /* All active paths are gone. Memory can now be cleared without resurrecting data. */
    ResetResult result = RESET_OK;
    for (int i = 0; i < 6; ++i) {
        if (moved[i] && !DeleteFileW(backups[i])) {
            *error = GetLastError();
            result = RESET_CLEANUP_PENDING;
        }
    }
    return result;
}
