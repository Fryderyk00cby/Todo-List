#ifndef STUDY_H
#define STUDY_H

#include <stdbool.h>
#include <wchar.h>
#include <windows.h>

#define STUDY_MAX_TEXT 256
#define STUDY_MAX_DATE 11   /* YYYY-MM-DD */
#define STUDY_MAX_TIME 9    /* HH:MM:SS */

typedef struct {
    char date[STUDY_MAX_DATE];
    char start_time[STUDY_MAX_TIME];
    char end_time[STUDY_MAX_TIME];
    int duration_sec;
    char task[STUDY_MAX_TEXT]; /* UTF-8 */
} StudySession;

typedef struct {
    StudySession *items;
    int count;
    int capacity;
} StudyLog;

void study_log_init(StudyLog *log);
void study_log_free(StudyLog *log);

bool study_get_data_path(wchar_t *buf, int bufsize);
bool study_load(StudyLog *log, const wchar_t *path);
bool study_save(const StudyLog *log, const wchar_t *path);

bool study_add_session(StudyLog *log, const char *date,
        const char *start_time, const char *end_time,
        int duration_sec, const char *task);

bool study_add_session_span(StudyLog *log, const SYSTEMTIME *start_st,
        const SYSTEMTIME *end_st, int total_duration_sec, const char *task);

bool study_remove_at(StudyLog *log, int index);

#define STUDY_MIN_SAVE_SEC 60

int study_total_seconds_for_date(const StudyLog *log, const char *date);
int study_sessions_for_date(const StudyLog *log, const char *date,
        StudySession *out, int max_out);

void study_format_duration(int seconds, wchar_t *out, int outcount);
void study_format_time_hm(const char *time_hms, wchar_t *out, int outcount);
void study_week_totals(const StudyLog *log, int week_offset,
        char dates[7][STUDY_MAX_DATE], int totals[7]);
void study_today_date(char *buf, int bufsize);
void study_systemtime_to_date(const SYSTEMTIME *st, char *buf, int bufsize);
void study_systemtime_to_time(const SYSTEMTIME *st, char *buf, int bufsize);
bool study_date_add_days(const char *date, int delta, char *out, int outsize);

#endif
