#include "study.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define STUDY_INITIAL_CAPACITY 32

static bool study_grow(StudyLog *log) {
    int new_cap = log->capacity == 0 ? STUDY_INITIAL_CAPACITY : log->capacity * 2;
    StudySession *new_items = (StudySession *)realloc(log->items, (size_t)new_cap * sizeof(StudySession));
    if (!new_items) {
        return false;
    }
    log->items = new_items;
    log->capacity = new_cap;
    return true;
}

static void study_sanitize_task(char *text) {
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

static bool study_is_valid_date(const char *value) {
    if (strlen(value) != 10 || value[4] != '-' || value[7] != '-') {
        return false;
    }
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (value[i] < '0' || value[i] > '9') {
            return false;
        }
    }
    int month = (value[5] - '0') * 10 + (value[6] - '0');
    int day = (value[8] - '0') * 10 + (value[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

static bool study_is_valid_time(const char *value) {
    if (strlen(value) != 8 || value[2] != ':' || value[5] != ':') {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        if (i == 2 || i == 5) {
            continue;
        }
        if (value[i] < '0' || value[i] > '9') {
            return false;
        }
    }
    int hour = (value[0] - '0') * 10 + (value[1] - '0');
    int minute = (value[3] - '0') * 10 + (value[4] - '0');
    int second = (value[6] - '0') * 10 + (value[7] - '0');
    return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59;
}

static void study_parse_line(const char *line, StudySession *session) {
    memset(session, 0, sizeof(*session));

    const char *p1 = strchr(line, '|');
    if (!p1) {
        return;
    }
    const char *p2 = strchr(p1 + 1, '|');
    if (!p2) {
        return;
    }
    const char *p3 = strchr(p2 + 1, '|');
    if (!p3) {
        return;
    }
    const char *p4 = strchr(p3 + 1, '|');
    if (!p4) {
        return;
    }

    size_t dlen = (size_t)(p1 - line);
    size_t slen = (size_t)(p2 - (p1 + 1));
    size_t elen = (size_t)(p3 - (p2 + 1));

    if (dlen >= STUDY_MAX_DATE) {
        dlen = STUDY_MAX_DATE - 1;
    }
    if (slen >= STUDY_MAX_TIME) {
        slen = STUDY_MAX_TIME - 1;
    }
    if (elen >= STUDY_MAX_TIME) {
        elen = STUDY_MAX_TIME - 1;
    }

    memcpy(session->date, line, dlen);
    session->date[dlen] = '\0';
    memcpy(session->start_time, p1 + 1, slen);
    session->start_time[slen] = '\0';
    memcpy(session->end_time, p2 + 1, elen);
    session->end_time[elen] = '\0';
    session->duration_sec = atoi(p3 + 1);
    strncpy(session->task, p4 + 1, STUDY_MAX_TEXT - 1);
    session->task[STUDY_MAX_TEXT - 1] = '\0';
    study_sanitize_task(session->task);
}

void study_log_init(StudyLog *log) {
    log->items = NULL;
    log->count = 0;
    log->capacity = 0;
}

void study_log_free(StudyLog *log) {
    free(log->items);
    log->items = NULL;
    log->count = 0;
    log->capacity = 0;
}

bool study_get_data_path(wchar_t *buf, int bufsize) {
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

    if ((int)(wcslen(exe_path) + wcslen(L"study.dat")) >= bufsize) {
        return false;
    }

    wcscpy(buf, exe_path);
    wcscat(buf, L"study.dat");
    return true;
}

void study_today_date(char *buf, int bufsize) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    study_systemtime_to_date(&st, buf, bufsize);
}

void study_systemtime_to_date(const SYSTEMTIME *st, char *buf, int bufsize) {
    snprintf(buf, (size_t)bufsize, "%04d-%02d-%02d", st->wYear, st->wMonth, st->wDay);
}

void study_systemtime_to_time(const SYSTEMTIME *st, char *buf, int bufsize) {
    snprintf(buf, (size_t)bufsize, "%02d:%02d:%02d", st->wHour, st->wMinute, st->wSecond);
}

bool study_date_add_days(const char *date, int delta, char *out, int outsize) {
    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));

    if (!study_is_valid_date(date)) {
        return false;
    }

    st.wYear = (WORD)((date[0] - '0') * 1000 + (date[1] - '0') * 100
            + (date[2] - '0') * 10 + (date[3] - '0'));
    st.wMonth = (WORD)((date[5] - '0') * 10 + (date[6] - '0'));
    st.wDay = (WORD)((date[8] - '0') * 10 + (date[9] - '0'));

    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
        return false;
    }

    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uli.QuadPart += (LONGLONG)delta * 24LL * 60LL * 60LL * 10000000LL;

    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;

    if (!FileTimeToSystemTime(&ft, &st)) {
        return false;
    }

    study_systemtime_to_date(&st, out, outsize);
    return true;
}

void study_format_duration(int seconds, wchar_t *out, int outcount) {
    if (seconds < 0) {
        seconds = 0;
    }
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    _snwprintf(out, outcount, L"%02d:%02d:%02d", hours, minutes, secs);
}

void study_format_time_hm(const char *time_hms, wchar_t *out, int outcount) {
    if (!time_hms || strlen(time_hms) < 5) {
        if (outcount > 0) {
            out[0] = L'\0';
        }
        return;
    }
    _snwprintf(out, outcount, L"%c%c:%c%c",
        time_hms[0], time_hms[1], time_hms[3], time_hms[4]);
}

void study_week_totals(const StudyLog *log, int week_offset,
        char dates[7][STUDY_MAX_DATE], int totals[7]) {
    SYSTEMTIME now;
    GetLocalTime(&now);

    char today[STUDY_MAX_DATE];
    study_systemtime_to_date(&now, today, sizeof(today));

    int from_monday = (now.wDayOfWeek + 6) % 7;
    char monday[STUDY_MAX_DATE];
    study_date_add_days(today, -from_monday, monday, sizeof(monday));

    if (week_offset != 0) {
        char shifted[STUDY_MAX_DATE];
        study_date_add_days(monday, week_offset * 7, shifted, sizeof(shifted));
        strncpy(monday, shifted, STUDY_MAX_DATE - 1);
        monday[STUDY_MAX_DATE - 1] = '\0';
    }

    for (int i = 0; i < 7; i++) {
        study_date_add_days(monday, i, dates[i], STUDY_MAX_DATE);
        totals[i] = study_total_seconds_for_date(log, dates[i]);
    }
}

bool study_load(StudyLog *log, const wchar_t *path) {
    FILE *fp = _wfopen(path, L"r");
    if (!fp) {
        return true;
    }

    char line[1024];
    bool first_line = true;

    while (fgets(line, sizeof(line), fp)) {
        char *start = line;

        if (first_line && (unsigned char)start[0] == 0xEF
                && (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF) {
            start += 3;
        }
        first_line = false;

        size_t len = strlen(start);
        while (len > 0 && (start[len - 1] == '\n' || start[len - 1] == '\r')) {
            start[--len] = '\0';
        }

        if (len < 20) {
            continue;
        }

        StudySession session;
        study_parse_line(start, &session);
        if (!study_is_valid_date(session.date) || !study_is_valid_time(session.start_time)
                || !study_is_valid_time(session.end_time) || session.task[0] == '\0') {
            continue;
        }

        if (log->count >= log->capacity && !study_grow(log)) {
            fclose(fp);
            return false;
        }

        log->items[log->count++] = session;
    }

    fclose(fp);
    return true;
}

bool study_save(const StudyLog *log, const wchar_t *path) {
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
    for (int i = 0; i < log->count; i++) {
        if (fprintf(fp, "%s|%s|%s|%d|%s\n",
                log->items[i].date,
                log->items[i].start_time,
                log->items[i].end_time,
                log->items[i].duration_sec,
                log->items[i].task) < 0) {
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

bool study_add_session(StudyLog *log, const char *date,
        const char *start_time, const char *end_time,
        int duration_sec, const char *task) {
    if (!date || !start_time || !end_time || !task || task[0] == '\0') {
        return false;
    }
    if (!study_is_valid_date(date) || !study_is_valid_time(start_time)
            || !study_is_valid_time(end_time) || duration_sec <= 0) {
        return false;
    }

    if (log->count >= log->capacity && !study_grow(log)) {
        return false;
    }

    StudySession *session = &log->items[log->count++];
    strncpy(session->date, date, STUDY_MAX_DATE - 1);
    session->date[STUDY_MAX_DATE - 1] = '\0';
    strncpy(session->start_time, start_time, STUDY_MAX_TIME - 1);
    session->start_time[STUDY_MAX_TIME - 1] = '\0';
    strncpy(session->end_time, end_time, STUDY_MAX_TIME - 1);
    session->end_time[STUDY_MAX_TIME - 1] = '\0';
    session->duration_sec = duration_sec;
    strncpy(session->task, task, STUDY_MAX_TEXT - 1);
    session->task[STUDY_MAX_TEXT - 1] = '\0';
    study_sanitize_task(session->task);

    return session->task[0] != '\0';
}

bool study_remove_at(StudyLog *log, int index) {
    if (index < 0 || index >= log->count) {
        return false;
    }

    for (int i = index; i < log->count - 1; i++) {
        log->items[i] = log->items[i + 1];
    }
    log->count--;
    return true;
}

int study_total_seconds_for_date(const StudyLog *log, const char *date) {
    int total = 0;
    for (int i = 0; i < log->count; i++) {
        if (strcmp(log->items[i].date, date) == 0) {
            total += log->items[i].duration_sec;
        }
    }
    return total;
}

int study_sessions_for_date(const StudyLog *log, const char *date,
        StudySession *out, int max_out) {
    int found = 0;
    for (int i = 0; i < log->count && found < max_out; i++) {
        if (strcmp(log->items[i].date, date) == 0) {
            out[found++] = log->items[i];
        }
    }
    return found;
}
