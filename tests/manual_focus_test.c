#include "../src/study.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SYSTEMTIME moment(int year, int month, int day, int hour, int minute) {
    SYSTEMTIME st = {0};
    st.wYear = (WORD)year; st.wMonth = (WORD)month; st.wDay = (WORD)day;
    st.wHour = (WORD)hour; st.wMinute = (WORD)minute;
    return st;
}

int main(void) {
    StudyLog log;
    study_log_init(&log);
    SYSTEMTIME a = moment(2024, 2, 28, 23, 30);
    SYSTEMTIME b = moment(2024, 2, 29, 1, 0);
    assert(study_add_manual_session(&log, &a, &b, "Offline reading"));
    assert(log.count == 2);
    assert(study_total_seconds_for_date(&log, "2024-02-28") == 1800);
    assert(study_total_seconds_for_date(&log, "2024-02-29") == 3600);
    assert(!study_add_manual_session(&log, &b, &a, "Reversed"));
    assert(!study_add_manual_session(&log, &a, &a, "Zero"));
    assert(!study_add_manual_session(&log, &a, &b, " \t\nignored"));
    SYSTEMTIME future = moment(2099, 1, 1, 0, 0);
    assert(!study_add_manual_session(&log, &a, &future, "Future"));
    assert(log.count == 2);
    a = moment(2024, 3, 1, 10, 0); b = moment(2024, 3, 1, 10, 1);
    assert(study_add_manual_session(&log, &a, &b, "One minute"));
    assert(log.items[2].duration_sec == 60);
    a = moment(2024, 3, 1, 23, 30); b = moment(2024, 4, 1, 23, 30);
    assert(study_add_manual_session(&log, &a, &b, "31 days"));
    assert(log.count == 35);
    int total = 0;
    for (int i = 3; i < log.count; ++i) total += log.items[i].duration_sec;
    assert(total == 31 * 86400);
    b.wDay = 2;
    assert(!study_add_manual_session(&log, &a, &b, "Too long"));
    assert(log.count == 35);
    a = moment(2024, 4, 2, 23, 30); b = moment(2024, 4, 3, 0, 0);
    assert(study_add_manual_session(&log, &a, &b, "Midnight"));
    assert(log.count == 36 && log.items[35].duration_sec == 1800);
    wchar_t temp[MAX_PATH], path[MAX_PATH];
    assert(GetTempPathW(MAX_PATH, temp));
    assert(GetTempFileNameW(temp, L"tdf", 0, path));
    assert(study_save(&log, path));
    StudyLog reloaded;
    study_log_init(&reloaded);
    assert(study_load(&reloaded, path));
    assert(reloaded.count == log.count);
    assert(study_total_seconds_for_date(&reloaded, "2024-02-29") == 3600);
    assert(strcmp(reloaded.items[0].task, "Offline reading") == 0);
    assert(DeleteFileW(path));
    study_log_free(&reloaded);
    study_log_free(&log);
    puts("PASS: manual intervals, leap-day split, limits, midnight, persistence and daily totals");
    return 0;
}
