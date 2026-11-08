#include "language.h"
#include <wchar.h>

static const wchar_t *const strings[TEXT_COUNT][LANGUAGE_COUNT] = {
#define X(key, english, chinese) {english, chinese},
#include "language_strings.h"
#undef X
};

AppLanguage app_language = LANGUAGE_ENGLISH;
static wchar_t settings_path[MAX_PATH];

void language_load(void) {
    app_language = LANGUAGE_ENGLISH;
    DWORD len = GetModuleFileNameW(NULL, settings_path, MAX_PATH);
    if (!len || len >= MAX_PATH) { settings_path[0] = 0; return; }
    wchar_t *slash = wcsrchr(settings_path, L'\\');
    if (!slash || slash - settings_path + 1 + wcslen(L"settings.ini") >= MAX_PATH) {
        settings_path[0] = 0;
        return;
    }
    wcscpy(slash + 1, L"settings.ini");
    wchar_t value[16];
    GetPrivateProfileStringW(L"General", L"Language", L"en", value, 16, settings_path);
    if (_wcsicmp(value, L"zh-CN") == 0) app_language = LANGUAGE_CHINESE;
}

BOOL language_save(void) {
    return settings_path[0] && WritePrivateProfileStringW(L"General", L"Language",
        app_language == LANGUAGE_CHINESE ? L"zh-CN" : L"en", settings_path);
}

const wchar_t *language_text(TextId id) {
    if (id < 0 || id >= TEXT_COUNT) return L"";
    return strings[id][app_language == LANGUAGE_CHINESE ? LANGUAGE_CHINESE : LANGUAGE_ENGLISH];
}

LCID language_locale(void) {
    return MAKELCID(MAKELANGID(app_language == LANGUAGE_CHINESE ? LANG_CHINESE : LANG_ENGLISH,
        app_language == LANGUAGE_CHINESE ? SUBLANG_CHINESE_SIMPLIFIED : SUBLANG_ENGLISH_US), SORT_DEFAULT);
}
