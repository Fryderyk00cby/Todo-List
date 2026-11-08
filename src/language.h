#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <windows.h>

typedef enum { LANGUAGE_ENGLISH, LANGUAGE_CHINESE, LANGUAGE_COUNT } AppLanguage;
typedef enum {
#define X(key, english, chinese) TEXT_##key,
#include "language_strings.h"
#undef X
    TEXT_COUNT
} TextId;

extern AppLanguage app_language;
void language_load(void);
BOOL language_save(void);
const wchar_t *language_text(TextId id);
LCID language_locale(void);
#define TR(key) language_text(TEXT_##key)

#endif
