#ifndef FACTORY_RESET_H
#define FACTORY_RESET_H

#include <windows.h>

typedef enum {
    RESET_OK,
    RESET_FAILED,
    RESET_RECOVERY_NEEDED,
    RESET_CLEANUP_PENDING
} ResetResult;

/* Only this application's named files in directory are affected. */
ResetResult factory_reset_files(const wchar_t *directory, DWORD *error);

#endif
