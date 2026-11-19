#define COBJMACROS
#include "music.h"
#include <oleauto.h>
#include "appearance.h"
#include "language.h"
#include <commdlg.h>
#include <commctrl.h>
#include <math.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#define MUSIC_LIMIT 1024
#define PATH_LIMIT 1024
#define LIST 1
#define ADD 2
#define REMOVE 3
#define PLAY 4
#define PREV 5
#define NEXT 6
#define HINT 7
#define STATUS 8
#define SEEK 9
#define CLOCK 10
#define REPEAT 11
#define PLAYLIST 12
#define NEW_PLAYLIST 13
#define RENAME_PLAYLIST 14
#define DELETE_PLAYLIST 15
#define PLAYLIST_LIMIT 32
#define NAME_LIMIT 80
#define MUSIC_FORMAT (-20261119)

typedef struct {
    wchar_t name[NAME_LIMIT];
    wchar_t (*paths)[PATH_LIMIT];
    int count;
} Playlist;

typedef struct {
    wchar_t (*paths)[PATH_LIMIT];
    wchar_t file[MAX_PATH];
    Playlist playlists[PLAYLIST_LIMIT];
    int playlist_count, active_playlist;
    int count, current, dpi;
    BOOL paused, load_failed, repeat_one, dragging;
    DWORD duration, position;
    MCIDEVICEID device;
    IDispatch *player, *controls;
    BOOL com_initialized, started;
    DWORD open_tick;
    HFONT font, hint_font;
    HBRUSH brush;
} Music;

/* Late binding keeps the build compatible with older MinGW SDKs. */
static HRESULT player_call(IDispatch *object, const wchar_t *name, WORD flags, VARIANT *arg, VARIANT *result) {
    if (!object) return E_POINTER;
    DISPID id, put = DISPID_PROPERTYPUT;
    LPOLESTR member = (LPOLESTR)name;
    HRESULT hr = IDispatch_GetIDsOfNames(object,&IID_NULL,&member,1,LOCALE_USER_DEFAULT,&id);
    if (FAILED(hr)) return hr;
    DISPPARAMS params = {0}; params.rgvarg = arg; params.cArgs = arg ? 1 : 0;
    if (flags & DISPATCH_PROPERTYPUT) { params.rgdispidNamedArgs = &put; params.cNamedArgs = 1; }
    VARIANT value; VariantInit(&value);
    EXCEPINFO exception = {0};
    hr = IDispatch_Invoke(object,id,&IID_NULL,LOCALE_USER_DEFAULT,flags,&params,&value,&exception,NULL);
    SysFreeString(exception.bstrSource); SysFreeString(exception.bstrDescription); SysFreeString(exception.bstrHelpFile);
    if (SUCCEEDED(hr) && result) *result = value; else VariantClear(&value);
    return hr;
}

static double player_number(IDispatch *object, const wchar_t *name) {
    VARIANT value, number; VariantInit(&value); VariantInit(&number);
    double result = 0;
    if (SUCCEEDED(player_call(object,name,DISPATCH_PROPERTYGET,NULL,&value)) &&
        SUCCEEDED(VariantChangeType(&number,&value,0,VT_R8))) result = V_R8(&number);
    VariantClear(&value); VariantClear(&number); return result;
}

static BOOL audio_open(Music *m) { return m->device || m->player; }

static void close_audio(Music *m) {
    if (m->player) player_call(m->player,L"close",DISPATCH_METHOD,NULL,NULL);
    if (m->controls) IDispatch_Release(m->controls);
    if (m->player) IDispatch_Release(m->player);
    m->controls = m->player = NULL; m->started = FALSE;
    if (m->device) mciSendCommandW(m->device, MCI_CLOSE, MCI_WAIT, 0);
    m->device = 0; m->paused = FALSE; m->dragging = FALSE; m->duration = m->position = 0;
}

static BOOL open_m4a(Music *m, const wchar_t *path) {
    CLSID clsid;
    if (FAILED(CLSIDFromProgID(L"WMPlayer.OCX",&clsid)) ||
        FAILED(CoCreateInstance(&clsid,NULL,CLSCTX_INPROC_SERVER,&IID_IDispatch,(void **)&m->player))) return FALSE;
    VARIANT controls; VariantInit(&controls);
    if (FAILED(player_call(m->player,L"controls",DISPATCH_PROPERTYGET,NULL,&controls)) || V_VT(&controls) != VT_DISPATCH) {
        VariantClear(&controls); close_audio(m); return FALSE;
    }
    m->controls = V_DISPATCH(&controls); /* Transfer the returned COM reference. */
    VARIANT url; VariantInit(&url); V_VT(&url) = VT_BSTR; V_BSTR(&url) = SysAllocString(path);
    HRESULT hr = V_BSTR(&url) ? player_call(m->player,L"URL",DISPATCH_PROPERTYPUT,&url,NULL) : E_OUTOFMEMORY;
    VariantClear(&url);
    if (SUCCEEDED(hr)) hr = player_call(m->controls,L"play",DISPATCH_METHOD,NULL,NULL);
    if (FAILED(hr)) { close_audio(m); return FALSE; }
    m->open_tick = GetTickCount(); return TRUE;
}

static BOOL save_playlist(Music *m) {
    wchar_t tmp[MAX_PATH + 8];
    if (m->load_failed || !m->file[0]) return FALSE;
    wcscpy(tmp, m->file); wcscat(tmp, L".tmp");
    FILE *f = _wfopen(tmp, L"wb");
    if (!f) return FALSE;
    m->playlists[m->active_playlist].paths = m->paths;
    m->playlists[m->active_playlist].count = m->count;
    int magic = MUSIC_FORMAT;
    BOOL ok = fwrite(&magic,sizeof(int),1,f) == 1 &&
        fwrite(&m->playlist_count,sizeof(int),1,f) == 1 &&
        fwrite(&m->active_playlist,sizeof(int),1,f) == 1;
    for (int i = 0; i < m->playlist_count && ok; ++i) {
        Playlist *list = &m->playlists[i];
        ok = fwrite(list->name,sizeof(list->name),1,f) == 1 &&
            fwrite(&list->count,sizeof(int),1,f) == 1 &&
            fwrite(list->paths,sizeof(*list->paths),list->count,f) == (size_t)list->count;
    }
    if (fclose(f)) ok = FALSE;
    if (ok && MoveFileExW(tmp, m->file, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return TRUE;
    DeleteFileW(tmp); return FALSE;
}

static BOOL valid_tracks(Playlist *list) {
    for (int i = 0; i < list->count; ++i)
        if (!list->paths[i][0] || !wmemchr(list->paths[i],0,PATH_LIMIT)) return FALSE;
    return TRUE;
}

static void load_playlists(Music *m) {
    m->playlist_count = 1;
    wcscpy(m->playlists[0].name,TR(MUSIC_DEFAULT_PLAYLIST));
    m->playlists[0].paths = m->paths;
    FILE *f = m->file[0] ? _wfopen(m->file,L"rb") : NULL;
    if (!f) {
        if (!m->file[0] || GetFileAttributesW(m->file) != INVALID_FILE_ATTRIBUTES || GetLastError() != ERROR_FILE_NOT_FOUND) m->load_failed = TRUE;
        return;
    }
    int header = 0;
    BOOL ok = fread(&header,sizeof(int),1,f) == 1;
    if (ok && header == MUSIC_FORMAT) {
        int count = 0, selected = 0;
        ok = fread(&count,sizeof(int),1,f) == 1 && count > 0 && count <= PLAYLIST_LIMIT &&
            fread(&selected,sizeof(int),1,f) == 1 && selected >= 0 && selected < count;
        if (ok) {
            m->playlist_count = count; m->active_playlist = selected;
            for (int i = 0; i < count && ok; ++i) {
                Playlist *list = &m->playlists[i];
                ok = fread(list->name,sizeof(list->name),1,f) == 1 && list->name[0] && wmemchr(list->name,0,NAME_LIMIT) &&
                    fread(&list->count,sizeof(int),1,f) == 1 && list->count >= 0 && list->count <= MUSIC_LIMIT;
                if (ok && !list->paths) list->paths = calloc(MUSIC_LIMIT,sizeof(*list->paths));
                if (ok) ok = list->paths && fread(list->paths,sizeof(*list->paths),list->count,f) == (size_t)list->count && valid_tracks(list);
            }
        }
    } else if (ok) {
        ok = header >= 0 && header <= MUSIC_LIMIT;
        if (ok) {
            m->playlists[0].count = header;
            ok = fread(m->paths,sizeof(*m->paths),header,f) == (size_t)header && valid_tracks(&m->playlists[0]);
        }
    }
    if (ok) ok = fgetc(f) == EOF && !ferror(f);
    fclose(f);
    if (!ok) {
        for (int i = 1; i < PLAYLIST_LIMIT; ++i) { free(m->playlists[i].paths); m->playlists[i].paths = NULL; }
        m->playlist_count = 1; m->active_playlist = 0; m->playlists[0].count = 0;
        wcscpy(m->playlists[0].name,TR(MUSIC_DEFAULT_PLAYLIST)); m->load_failed = TRUE;
    }
    m->paths = m->playlists[m->active_playlist].paths;
    m->count = m->playlists[m->active_playlist].count;
}

static void refresh_playlists(HWND hwnd, Music *m) {
    HWND combo = GetDlgItem(hwnd,PLAYLIST);
    SendMessageW(combo,CB_RESETCONTENT,0,0);
    for (int i = 0; i < m->playlist_count; ++i)
        SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)m->playlists[i].name);
    SendMessageW(combo,CB_SETCURSEL,m->active_playlist,0);
    EnableWindow(combo,!m->load_failed);
    EnableWindow(GetDlgItem(hwnd,NEW_PLAYLIST),!m->load_failed && m->playlist_count < PLAYLIST_LIMIT);
    EnableWindow(GetDlgItem(hwnd,RENAME_PLAYLIST),!m->load_failed);
    EnableWindow(GetDlgItem(hwnd,DELETE_PLAYLIST),!m->load_failed && m->playlist_count > 1);
}

static void save_or_warn(HWND hwnd, Music *m) {
    if (!save_playlist(m)) MessageBoxW(hwnd, TR(MUSIC_SAVE_ERROR), TR(MUSIC_TAB), MB_OK | MB_ICONWARNING);
}

static const wchar_t *basename_w(const wchar_t *path) {
    const wchar_t *name = wcsrchr(path, L'\\');
    return name ? name + 1 : path;
}

static void update_progress(HWND hwnd, Music *m) {
    if (m->device && !m->dragging) {
        MCI_STATUS_PARMS status = {0}; status.dwItem = MCI_STATUS_POSITION;
        if (!mciSendCommandW(m->device, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)&status))
            m->position = (DWORD)status.dwReturn;
    }
    if (m->player) {
        VARIANT media; VariantInit(&media);
        if (SUCCEEDED(player_call(m->player,L"currentMedia",DISPATCH_PROPERTYGET,NULL,&media)) && V_VT(&media) == VT_DISPATCH)
            m->duration = (DWORD)(player_number(V_DISPATCH(&media),L"duration")*1000);
        VariantClear(&media);
        if (!m->dragging) m->position = (DWORD)(player_number(m->controls,L"currentPosition")*1000);
    }
    DWORD shown = m->position;
    SendDlgItemMessageW(hwnd, SEEK, TBM_SETRANGEMAX, FALSE, m->duration);
    if (m->dragging) shown = (DWORD)SendDlgItemMessageW(hwnd, SEEK, TBM_GETPOS, 0, 0);
    else SendDlgItemMessageW(hwnd, SEEK, TBM_SETPOS, TRUE, shown);
    wchar_t time[80];
    wsprintfW(time, L"%02lu:%02lu   /   %02lu:%02lu", shown/60000, shown/1000%60,
        m->duration/60000, m->duration/1000%60);
    SetWindowTextW(GetDlgItem(hwnd, CLOCK), time);
    EnableWindow(GetDlgItem(hwnd, SEEK), audio_open(m) && m->duration);
    InvalidateRect(GetDlgItem(hwnd, LIST), NULL, FALSE);
}

static void seek_audio(HWND hwnd, Music *m, DWORD position) {
    if (!audio_open(m) || !m->duration) return;
    if (position >= m->duration) position = m->duration > 1 ? m->duration-1 : 0;
    if (m->player) {
        VARIANT value; VariantInit(&value); V_VT(&value) = VT_R8; V_R8(&value) = position/1000.0;
        if (SUCCEEDED(player_call(m->controls,L"currentPosition",DISPATCH_PROPERTYPUT,&value,NULL))) m->position = position;
        else SetWindowTextW(GetDlgItem(hwnd,STATUS),TR(MUSIC_ERROR));
        update_progress(hwnd,m); return;
    }
    BOOL paused = m->paused;
    MCI_SEEK_PARMS seek = {0}; seek.dwTo = position;
    MCIERROR error = mciSendCommandW(m->device, MCI_SEEK, MCI_TO | MCI_WAIT, (DWORD_PTR)&seek);
    if (!error) {
        MCI_PLAY_PARMS play = {0}; play.dwCallback = (DWORD_PTR)hwnd;
        error = mciSendCommandW(m->device, MCI_PLAY, MCI_NOTIFY, (DWORD_PTR)&play);
        if (!error && paused) error = mciSendCommandW(m->device, MCI_PAUSE, MCI_WAIT, 0);
    }
    if (!error) { m->position = position; m->paused = paused; }
    else SetWindowTextW(GetDlgItem(hwnd, STATUS), TR(MUSIC_ERROR));
    update_progress(hwnd,m);
}

static void refresh(HWND hwnd, Music *m) {
    HWND list = GetDlgItem(hwnd, LIST);
    int selected = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < m->count; ++i)
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)basename_w(m->paths[i]));
    if (selected >= m->count) selected = m->count - 1;
    SendMessageW(list, LB_SETCURSEL, selected, 0);
    SetWindowTextW(GetDlgItem(hwnd, PLAY), audio_open(m) && !m->paused ? TR(MUSIC_PAUSE) : TR(MUSIC_PLAY));
    SetWindowTextW(GetDlgItem(hwnd, STATUS), m->current >= 0 ? basename_w(m->paths[m->current]) : L"");
    SetWindowTextW(GetDlgItem(hwnd, REPEAT), m->repeat_one ? TR(MUSIC_REPEAT_ONE) : TR(MUSIC_REPEAT_ALL));
    InvalidateRect(GetDlgItem(hwnd, PLAY), NULL, FALSE);
    InvalidateRect(GetDlgItem(hwnd, REPEAT), NULL, FALSE);
    update_progress(hwnd, m);
    EnableWindow(GetDlgItem(hwnd, PLAY), m->count > 0);
    EnableWindow(GetDlgItem(hwnd, PREV), m->count > 0);
    EnableWindow(GetDlgItem(hwnd, NEXT), m->count > 0);
    EnableWindow(GetDlgItem(hwnd, REMOVE), m->count > 0 && !m->load_failed);
    EnableWindow(GetDlgItem(hwnd, ADD), !m->load_failed && m->count < MUSIC_LIMIT);
}

/* Try each track at most once; an all-invalid playlist must never spin. */
static void play_track(HWND hwnd, Music *m, int index, int direction) {
    close_audio(m);
    for (int tried = 0; tried < m->count; ++tried) {
        index = (index + m->count) % m->count;
        const wchar_t *extension = wcsrchr(m->paths[index],L'.');
        if (extension && !_wcsicmp(extension,L".m4a")) {
            if (open_m4a(m,m->paths[index])) {
                m->current = index;
                SendDlgItemMessageW(hwnd,LIST,LB_SETCURSEL,index,0);
                refresh(hwnd,m); return;
            }
            index += direction; continue;
        }
        MCI_OPEN_PARMSW open = {0};
        open.lpstrElementName = m->paths[index];
        if (!mciSendCommandW(0, MCI_OPEN, MCI_OPEN_ELEMENT | MCI_WAIT, (DWORD_PTR)&open)) {
            m->device = open.wDeviceID;
            MCI_SET_PARMS format = {0}; format.dwTimeFormat = MCI_FORMAT_MILLISECONDS;
            if (mciSendCommandW(m->device,MCI_SET,MCI_SET_TIME_FORMAT | MCI_WAIT,(DWORD_PTR)&format)) {
                close_audio(m); index += direction; continue;
            }
            MCI_STATUS_PARMS length = {0}; length.dwItem = MCI_STATUS_LENGTH;
            if (!mciSendCommandW(m->device,MCI_STATUS,MCI_STATUS_ITEM | MCI_WAIT,(DWORD_PTR)&length))
                m->duration = (DWORD)length.dwReturn;
            MCI_PLAY_PARMS play = {0}; play.dwCallback = (DWORD_PTR)hwnd;
            if (!mciSendCommandW(m->device, MCI_PLAY, MCI_NOTIFY, (DWORD_PTR)&play)) {
                m->current = index;
                SendDlgItemMessageW(hwnd, LIST, LB_SETCURSEL, index, 0);
                refresh(hwnd, m); return;
            }
            close_audio(m);
        }
        index += direction;
    }
    m->current = -1; refresh(hwnd, m);
    SetWindowTextW(GetDlgItem(hwnd, STATUS), TR(MUSIC_ERROR));
}

static void add_audio(HWND hwnd, Music *m) {
    wchar_t *buffer = calloc(65536, sizeof(wchar_t));
    if (!buffer) return;
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Audio (*.mp3;*.wav;*.wma;*.mid;*.m4a)\0*.mp3;*.wav;*.wma;*.mid;*.m4a\0All files\0*.*\0\0";
    ofn.lpstrFile = buffer; ofn.nMaxFile = 65536;
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        const wchar_t *next = buffer + wcslen(buffer) + 1;
        BOOL multiple = *next != 0;
        do {
            wchar_t path[PATH_LIMIT];
            if (multiple) {
                if (wcslen(buffer) + wcslen(next) + 2 > PATH_LIMIT) { next += wcslen(next) + 1; continue; }
                wcscpy(path, buffer); wcscat(path, L"\\"); wcscat(path, next);
            } else {
                if (wcslen(buffer) >= PATH_LIMIT) break;
                wcscpy(path, buffer);
            }
            int i = 0;
            while (i < m->count && _wcsicmp(path, m->paths[i])) ++i;
            if (i == m->count && m->count < MUSIC_LIMIT) wcscpy(m->paths[m->count++], path);
            if (!multiple) break;
            next += wcslen(next) + 1;
        } while (*next && m->count < MUSIC_LIMIT);
        save_or_warn(hwnd, m); refresh(hwnd, m);
    }
    free(buffer);
}

static void draw_music_item(const DRAWITEMSTRUCT *item, Music *m) {
    HDC dc = item->hDC; int saved = SaveDC(dc);
    RECT r = item->rcItem;
    FillRect(dc,&r,m->brush);
    SelectObject(dc,m->font); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,THEME.ink);
    int id = (int)item->CtlID, cx = (r.left+r.right)/2, cy = (r.top+r.bottom)/2;
    int d = MulDiv(1,m->dpi,96); if (d < 1) d = 1;
    if (item->CtlType == ODT_LISTBOX) {
        int i = (int)item->itemID;
        if (i < 0 || i >= m->count) { RestoreDC(dc,saved); return; }
        BOOL active = i == m->current;
        RECT card = r; InflateRect(&card,-2*d,-3*d);
        appearance_round_rect(dc,card,active || (item->itemState & ODS_SELECTED) ? THEME.tint : THEME.surface,THEME.border,12*d);
        RECT number = card; number.left += 12*d; number.right = number.left+30*d;
        wchar_t n[16]; wsprintfW(n,L"%02d",i+1);
        SetTextColor(dc,active ? THEME.accent : THEME.muted);
        DrawTextW(dc,n,-1,&number,DT_SINGLELINE | DT_VCENTER);
        RECT title = card; title.left += 48*d; title.right -= 16*d; title.bottom -= active ? 5*d : 0;
        SetTextColor(dc,THEME.ink);
        DrawTextW(dc,basename_w(m->paths[i]),-1,&title,DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (active) {
            RECT rail = {title.left,card.bottom-8*d,title.right,card.bottom-5*d};
            appearance_round_rect(dc,rail,THEME.border,THEME.border,3*d);
            if (m->duration) {
                rail.right = rail.left+(int)((double)(rail.right-rail.left)*m->position/m->duration);
                if (rail.right > title.right) rail.right = title.right;
                if (rail.right > rail.left) appearance_round_rect(dc,rail,THEME.accent,THEME.accent,3*d);
            }
        }
        if (item->itemState & ODS_FOCUS) { InflateRect(&card,-3*d,-3*d); DrawFocusRect(dc,&card); }
        RestoreDC(dc,saved); return;
    }
    BOOL disabled = item->itemState & ODS_DISABLED;
    COLORREF ink = disabled ? THEME.border : THEME.ink;
    if (id == PLAY) {
        int radius = (r.right-r.left)/2-4*d;
        /* Small colored segments make a smooth orange/pink/purple ring. */
        for (int i = 0; i < 120; ++i) {
            double a = i*6.28318530718/120.0, b = (i+1)*6.28318530718/120.0;
            COLORREF color = disabled ? THEME.border : RGB(235-i,110-i*65/120,40+i*145/120);
            HPEN pen = CreatePen(PS_SOLID,5*d,color); HGDIOBJ old = SelectObject(dc,pen);
            MoveToEx(dc,cx+(int)(cos(a)*radius),cy+(int)(sin(a)*radius),NULL);
            LineTo(dc,cx+(int)(cos(b)*radius),cy+(int)(sin(b)*radius));
            SelectObject(dc,old); DeleteObject(pen);
        }
        if (item->itemState & ODS_SELECTED) ink = THEME.accent;
        HBRUSH fill = CreateSolidBrush(ink); HGDIOBJ oldb = SelectObject(dc,fill), oldp = SelectObject(dc,GetStockObject(NULL_PEN));
        if (audio_open(m) && !m->paused) {
            RoundRect(dc,cx-11*d,cy-12*d,cx-3*d,cy+12*d,4*d,4*d);
            RoundRect(dc,cx+3*d,cy-12*d,cx+11*d,cy+12*d,4*d,4*d);
        } else {
            POINT triangle[] = {{cx-7*d,cy-13*d},{cx-7*d,cy+13*d},{cx+13*d,cy}};
            Polygon(dc,triangle,3);
        }
        SelectObject(dc,oldb); SelectObject(dc,oldp); DeleteObject(fill);
    } else if (id == PREV || id == NEXT) {
        int sign = id == NEXT ? 1 : -1;
        HBRUSH fill = CreateSolidBrush(ink); HGDIOBJ oldb = SelectObject(dc,fill), oldp = SelectObject(dc,GetStockObject(NULL_PEN));
        POINT triangle[] = {{cx-sign*7*d,cy-9*d},{cx-sign*7*d,cy+9*d},{cx+sign*7*d,cy}};
        Polygon(dc,triangle,3);
        int x = cx+sign*10*d; Rectangle(dc,x-1*d,cy-9*d,x+2*d,cy+9*d);
        SelectObject(dc,oldb); SelectObject(dc,oldp); DeleteObject(fill);
    } else if (id == REPEAT) {
        RECT button = r; InflateRect(&button,-2*d,-2*d);
        COLORREF color = disabled ? THEME.border : m->repeat_one ? THEME.accent : THEME.ink;
        appearance_round_rect(dc,button,m->repeat_one || (item->itemState & ODS_SELECTED) ? THEME.tint : THEME.surface,
            THEME.border,12*d);
        HPEN pen = CreatePen(PS_SOLID,2*d,color);
        HGDIOBJ oldp = SelectObject(dc,pen);
        /* Two opposing arrows form the repeat symbol without a font dependency. */
        MoveToEx(dc,cx-11*d,cy-1*d,NULL);
        LineTo(dc,cx-11*d,cy-7*d); LineTo(dc,cx+10*d,cy-7*d);
        MoveToEx(dc,cx+6*d,cy-11*d,NULL);
        LineTo(dc,cx+10*d,cy-7*d); LineTo(dc,cx+6*d,cy-3*d);
        MoveToEx(dc,cx+11*d,cy+1*d,NULL);
        LineTo(dc,cx+11*d,cy+7*d); LineTo(dc,cx-10*d,cy+7*d);
        MoveToEx(dc,cx-6*d,cy+3*d,NULL);
        LineTo(dc,cx-10*d,cy+7*d); LineTo(dc,cx-6*d,cy+11*d);
        SelectObject(dc,oldp); DeleteObject(pen);
        if (m->repeat_one) {
            HFONT number_font = m->hint_font ? m->hint_font : m->font;
            SelectObject(dc,number_font); SetTextColor(dc,color);
            RECT number = {cx-5*d,cy-7*d,cx+5*d,cy+7*d};
            DrawTextW(dc,L"1",1,&number,DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }
    } else {
        RECT button = r; InflateRect(&button,-2*d,-2*d);
        BOOL accent = id == REPEAT && m->repeat_one;
        appearance_round_rect(dc,button,accent || (item->itemState & ODS_SELECTED) ? THEME.tint : THEME.surface,
            THEME.border,12*d);
        SetTextColor(dc,disabled ? THEME.muted : accent ? THEME.accent : THEME.ink);
        wchar_t text[128]; GetWindowTextW(item->hwndItem,text,128);
        DrawTextW(dc,text,-1,&button,DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    }
    if (item->itemState & ODS_FOCUS) { InflateRect(&r,-3*d,-3*d); DrawFocusRect(dc,&r); }
    RestoreDC(dc,saved);
}

static LRESULT CALLBACK music_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Music *m = (Music *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        m = calloc(1, sizeof(*m));
        if (!m) return -1;
        m->paths = calloc(MUSIC_LIMIT, sizeof(*m->paths));
        if (!m->paths) { free(m); return -1; }
        m->current = -1; m->dpi = 96;
        m->com_initialized = SUCCEEDED(CoInitializeEx(NULL,COINIT_APARTMENTTHREADED));
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)m);
        DWORD n = GetModuleFileNameW(NULL, m->file, MAX_PATH);
        wchar_t *slash = wcsrchr(m->file, L'\\');
        if (!n || n >= MAX_PATH || !slash || slash - m->file + 11 >= MAX_PATH) m->file[0] = 0;
        else wcscpy(slash + 1, L"music.dat");
        load_playlists(m);
        CreateWindowW(L"COMBOBOX",L"",WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL,
            0,0,0,0,hwnd,(HMENU)PLAYLIST,NULL,NULL);
        SendDlgItemMessageW(hwnd,PLAYLIST,CB_LIMITTEXT,NAME_LIMIT-1,0);
        for (int i = NEW_PLAYLIST; i <= DELETE_PLAYLIST; ++i)
            CreateWindowW(L"BUTTON",L"",WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,0,0,0,0,hwnd,(HMENU)(INT_PTR)i,NULL,NULL);
        refresh_playlists(hwnd,m);
        for (int i = ADD; i <= NEXT; ++i)
            CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0,0,0,0, hwnd, (HMENU)(INT_PTR)i, NULL, NULL);
        for (int i = REPEAT; i <= REPEAT; ++i)
            CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,0,0,0,0,hwnd,(HMENU)(INT_PTR)i,NULL,NULL);
        CreateWindowW(TRACKBAR_CLASSW,L"",WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,0,0,0,0,hwnd,(HMENU)SEEK,NULL,NULL);
        CreateWindowW(L"STATIC",L"",WS_CHILD | WS_VISIBLE | SS_CENTER,0,0,0,0,hwnd,(HMENU)CLOCK,NULL,NULL);
        m->brush = CreateSolidBrush(THEME.surface);
        SetTimer(hwnd,1,250,NULL);
        CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 0,0,0,0, hwnd, (HMENU)LIST, NULL, NULL);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0,0,0,0, hwnd, (HMENU)HINT, NULL, NULL);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS, 0,0,0,0, hwnd, (HMENU)STATUS, NULL, NULL);
        refresh(hwnd, m);
        return 0;
    }
    case WM_SIZE: {
        int pad = MulDiv(18,m->dpi,96), gap = MulDiv(8,m->dpi,96);
        int width = LOWORD(lp), height = HIWORD(lp), center = width/2;
        int button = MulDiv(42,m->dpi,96), big = MulDiv(68,m->dpi,96);
        int top = MulDiv(138,m->dpi,96);
        int tool = MulDiv(72,m->dpi,96), tool_height = MulDiv(30,m->dpi,96);
        int combo_width = width-pad*2-(tool+gap)*3;
        MoveWindow(GetDlgItem(hwnd,PLAYLIST),pad,pad,combo_width,MulDiv(220,m->dpi,96),TRUE);
        for (int i = 0; i < 3; ++i)
            MoveWindow(GetDlgItem(hwnd,NEW_PLAYLIST+i),pad+combo_width+gap+i*(tool+gap),pad,tool,tool_height,TRUE);
        MoveWindow(GetDlgItem(hwnd,STATUS),pad,MulDiv(62,m->dpi,96),width-pad*2,MulDiv(26,m->dpi,96),TRUE);
        MoveWindow(GetDlgItem(hwnd,SEEK),pad,MulDiv(94,m->dpi,96),width-pad*2,MulDiv(24,m->dpi,96),TRUE);
        MoveWindow(GetDlgItem(hwnd,CLOCK),pad,MulDiv(118,m->dpi,96),width-pad*2,MulDiv(20,m->dpi,96),TRUE);
        MoveWindow(GetDlgItem(hwnd,PLAY),center-big/2,top,big,big,TRUE);
        MoveWindow(GetDlgItem(hwnd,PREV),center-big/2-gap*2-button,top+(big-button)/2,button,button,TRUE);
        MoveWindow(GetDlgItem(hwnd,NEXT),center+big/2+gap*2,top+(big-button)/2,button,button,TRUE);
        int row = top+big+gap, action = MulDiv(110,m->dpi,96), bh = MulDiv(32,m->dpi,96);
        MoveWindow(GetDlgItem(hwnd,ADD),pad,row,action,bh,TRUE);
        MoveWindow(GetDlgItem(hwnd,REMOVE),pad+action+gap,row,action,bh,TRUE);
        MoveWindow(GetDlgItem(hwnd,REPEAT),width-pad-MulDiv(44,m->dpi,96),row,MulDiv(44,m->dpi,96),bh,TRUE);
        int list_top = row+bh+gap;
        int hint_height = MulDiv(32,m->dpi,96);
        int hint_top = height-pad-hint_height;
        if (hint_top < list_top) hint_top = list_top;
        MoveWindow(GetDlgItem(hwnd,LIST),pad,list_top,width-pad*2,hint_top > list_top+gap ? hint_top-list_top-gap : 0,TRUE);
        MoveWindow(GetDlgItem(hwnd,HINT),pad,hint_top,width-pad*2,hint_height,TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int selected = (int)SendDlgItemMessageW(hwnd, LIST, LB_GETCURSEL, 0, 0);
        switch (LOWORD(wp)) {
        case PLAYLIST:
            if (HIWORD(wp) == CBN_SELCHANGE && !m->load_failed) {
                int index = (int)SendDlgItemMessageW(hwnd,PLAYLIST,CB_GETCURSEL,0,0);
                if (index >= 0 && index < m->playlist_count && index != m->active_playlist) {
                    if (!save_playlist(m)) { save_or_warn(hwnd,m); refresh_playlists(hwnd,m); break; }
                    int previous = m->active_playlist;
                    m->active_playlist = index;
                    m->paths = m->playlists[index].paths; m->count = m->playlists[index].count;
                    if (!save_playlist(m)) {
                        m->active_playlist = previous;
                        m->paths = m->playlists[previous].paths; m->count = m->playlists[previous].count;
                        MessageBoxW(hwnd,TR(MUSIC_SAVE_ERROR),TR(MUSIC_TAB),MB_OK | MB_ICONWARNING);
                    } else { close_audio(m); m->current = -1; }
                    refresh_playlists(hwnd,m); refresh(hwnd,m);
                }
            }
            break;
        case NEW_PLAYLIST:
        case RENAME_PLAYLIST:
        case DELETE_PLAYLIST: {
            if (m->load_failed) break;
            int action = LOWORD(wp), previous = m->active_playlist;
            wchar_t name[NAME_LIMIT]; GetDlgItemTextW(hwnd,PLAYLIST,name,NAME_LIMIT);
            size_t length = wcslen(name);
            while (length && iswspace(name[length-1])) name[--length] = 0;
            wchar_t *start = name; while (iswspace(*start)) ++start;
            if (start != name) memmove(name,start,(wcslen(start)+1)*sizeof(wchar_t));
            if (action != DELETE_PLAYLIST && !name[0]) break;
            if (!save_playlist(m)) { save_or_warn(hwnd,m); break; }
            if (action == NEW_PLAYLIST && m->playlist_count < PLAYLIST_LIMIT) {
                /* An unchanged selector name means create a fresh numbered playlist. */
                if (!wcscmp(name,m->playlists[previous].name)) wsprintfW(name,TR(MUSIC_NEW_NAME),m->playlist_count+1);
                BOOL duplicate = FALSE;
                for (int i = 0; i < m->playlist_count; ++i) if (!_wcsicmp(name,m->playlists[i].name)) duplicate = TRUE;
                if (duplicate) { MessageBoxW(hwnd,TR(MUSIC_DUPLICATE),TR(MUSIC_TAB),MB_OK); break; }
                Playlist *list = &m->playlists[m->playlist_count];
                list->paths = calloc(MUSIC_LIMIT,sizeof(*list->paths));
                if (!list->paths) break;
                wcscpy(list->name,name); list->count = 0;
                m->active_playlist = m->playlist_count++; m->paths = list->paths; m->count = 0;
                if (!save_playlist(m)) {
                    free(list->paths); memset(list,0,sizeof(*list)); --m->playlist_count;
                    m->active_playlist = previous; m->paths = m->playlists[previous].paths; m->count = m->playlists[previous].count;
                    MessageBoxW(hwnd,TR(MUSIC_SAVE_ERROR),TR(MUSIC_TAB),MB_OK | MB_ICONWARNING);
                } else { close_audio(m); m->current = -1; }
            } else if (action == RENAME_PLAYLIST) {
                BOOL duplicate = FALSE;
                for (int i = 0; i < m->playlist_count; ++i) if (i != previous && !_wcsicmp(name,m->playlists[i].name)) duplicate = TRUE;
                if (duplicate) { MessageBoxW(hwnd,TR(MUSIC_DUPLICATE),TR(MUSIC_TAB),MB_OK); break; }
                wchar_t old[NAME_LIMIT]; wcscpy(old,m->playlists[previous].name);
                wcscpy(m->playlists[previous].name,name);
                if (!save_playlist(m)) { wcscpy(m->playlists[previous].name,old); MessageBoxW(hwnd,TR(MUSIC_SAVE_ERROR),TR(MUSIC_TAB),MB_OK | MB_ICONWARNING); }
            } else if (action == DELETE_PLAYLIST && m->playlist_count > 1) {
                if (MessageBoxW(hwnd,TR(MUSIC_DELETE_CONFIRM),TR(MUSIC_TAB),MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) break;
                Playlist removed = m->playlists[previous];
                memmove(m->playlists+previous,m->playlists+previous+1,(m->playlist_count-previous-1)*sizeof(Playlist));
                --m->playlist_count; memset(&m->playlists[m->playlist_count],0,sizeof(Playlist));
                m->active_playlist = previous < m->playlist_count ? previous : m->playlist_count-1;
                m->paths = m->playlists[m->active_playlist].paths; m->count = m->playlists[m->active_playlist].count;
                if (!save_playlist(m)) {
                    memmove(m->playlists+previous+1,m->playlists+previous,(m->playlist_count-previous)*sizeof(Playlist));
                    m->playlists[previous] = removed; ++m->playlist_count; m->active_playlist = previous;
                    m->paths = removed.paths; m->count = removed.count;
                    MessageBoxW(hwnd,TR(MUSIC_SAVE_ERROR),TR(MUSIC_TAB),MB_OK | MB_ICONWARNING);
                } else { close_audio(m); m->current = -1; free(removed.paths); }
            }
            refresh_playlists(hwnd,m); refresh(hwnd,m); break;
        }
        case REPEAT: m->repeat_one = !m->repeat_one; refresh(hwnd,m); break;
        case ADD: add_audio(hwnd, m); break;
        case REMOVE:
            if (selected < 0 || m->load_failed) break;
            if (selected == m->current) { close_audio(m); m->current = -1; }
            else if (selected < m->current) --m->current;
            memmove(m->paths+selected, m->paths+selected+1, (m->count-selected-1)*sizeof(*m->paths));
            --m->count; save_or_warn(hwnd,m); refresh(hwnd,m); break;
        case LIST:
            if (HIWORD(wp) == LBN_DBLCLK && selected >= 0) play_track(hwnd,m,selected,1);
            break;
        case PLAY:
            if (audio_open(m)) {
                MCIERROR error = m->player ? (FAILED(player_call(m->controls,m->paused ? L"play" : L"pause",DISPATCH_METHOD,NULL,NULL)) ? 1 : 0) : mciSendCommandW(m->device, m->paused ? MCI_RESUME : MCI_PAUSE, MCI_WAIT, 0);
                if (!error) m->paused = !m->paused;
                else close_audio(m);
                refresh(hwnd,m);
                if (error) SetWindowTextW(GetDlgItem(hwnd, STATUS), TR(MUSIC_ERROR));
            } else if (m->count) play_track(hwnd,m, selected >= 0 ? selected : 0,1);
            break;
        case PREV: if (m->count) play_track(hwnd,m,m->current >= 0 ? m->current-1 : 0,-1); break;
        case NEXT: if (m->count) play_track(hwnd,m,m->current >= 0 ? m->current+1 : 0,1); break;
        }
        return 0;
    }
    case WM_TIMER:
        update_progress(hwnd,m);
        if (m->player && !m->paused) {
            int state = (int)player_number(m->player,L"playState");
            if (state == 3) m->started = TRUE;
            if (state == 8 || (state == 1 && m->started && m->duration && m->position+500 >= m->duration))
                play_track(hwnd,m,m->repeat_one ? m->current : m->current+1,1);
            else if ((state == 1 && m->started) || (!m->started && GetTickCount()-m->open_tick > 15000)) {
                close_audio(m); refresh(hwnd,m);
                SetWindowTextW(GetDlgItem(hwnd,STATUS),TR(MUSIC_ERROR));
            }
        }
        return 0;
    case WM_HSCROLL:
        if ((HWND)lp == GetDlgItem(hwnd,SEEK)) {
            if (LOWORD(wp) == TB_THUMBTRACK) { m->dragging = TRUE; update_progress(hwnd,m); }
            else if (LOWORD(wp) != TB_ENDTRACK || m->dragging) {
                DWORD position = (DWORD)SendDlgItemMessageW(hwnd,SEEK,TBM_GETPOS,0,0);
                m->dragging = FALSE; seek_audio(hwnd,m,position);
            }
        }
        return 0;
    case WM_DRAWITEM:
        draw_music_item((const DRAWITEMSTRUCT *)lp,m); return TRUE;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT *)lp)->itemHeight = MulDiv(52, m ? m->dpi : 96,96); return TRUE;
    case MM_MCINOTIFY:
        if (m->device == (MCIDEVICEID)lp && wp == MCI_NOTIFY_SUCCESSFUL && m->count)
            play_track(hwnd,m,m->repeat_one ? m->current : m->current+1,1);
        else if (m->device == (MCIDEVICEID)lp && wp == MCI_NOTIFY_FAILURE) {
            close_audio(m); m->current = -1; refresh(hwnd,m);
            SetWindowTextW(GetDlgItem(hwnd, STATUS), TR(MUSIC_ERROR));
        }
        return 0;
    case WM_CTLCOLORBTN: case WM_CTLCOLORSTATIC: case WM_CTLCOLORLISTBOX: {
        COLORREF ink = THEME.ink;
        if ((HWND)lp == GetDlgItem(hwnd,HINT))
            ink = m->load_failed ? THEME.muted : RGB((GetRValue(THEME.muted)*2+GetRValue(THEME.surface))/3,
                (GetGValue(THEME.muted)*2+GetGValue(THEME.surface))/3,
                (GetBValue(THEME.muted)*2+GetBValue(THEME.surface))/3);
        SetTextColor((HDC)wp, ink); SetBkColor((HDC)wp, THEME.surface);
        return (LRESULT)m->brush;
    }
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd,&rc); FillRect((HDC)wp,&rc,m->brush); return 1;
    }
    case WM_DESTROY:
        KillTimer(hwnd,1); close_audio(m); DeleteObject(m->brush); DeleteObject(m->hint_font);
        for (int i = 0; i < m->playlist_count; ++i) free(m->playlists[i].paths);
        if (m->com_initialized) CoUninitialize();
        free(m);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

HWND music_create(HWND parent, int id) {
    WNDCLASSW wc = {0}; wc.lpfnWndProc = music_proc; wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"TodoMusic"; wc.hCursor = LoadCursor(NULL,IDC_ARROW);
    RegisterClassW(&wc);
    return CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"",WS_CHILD | WS_CLIPCHILDREN,0,0,0,0,parent,(HMENU)(INT_PTR)id,wc.hInstance,NULL);
}

void music_style(HWND hwnd, int dpi, HFONT font) {
    Music *m = (Music *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if (!m) return;
    m->dpi = dpi; m->font = font;
    if (m->brush) DeleteObject(m->brush);
    m->brush = CreateSolidBrush(THEME.surface);
    const TextId labels[] = {TEXT_MUSIC_ADD,TEXT_MUSIC_REMOVE,TEXT_MUSIC_PLAY,TEXT_MUSIC_PREV,TEXT_MUSIC_NEXT};
    for (int i = ADD; i <= NEXT; ++i) SetWindowTextW(GetDlgItem(hwnd,i),language_text(labels[i-ADD]));
    SetWindowTextW(GetDlgItem(hwnd,HINT),m->load_failed ? TR(MUSIC_LOAD_ERROR) : TR(MUSIC_HINT));
    SendDlgItemMessageW(hwnd,LIST,LB_SETITEMHEIGHT,0,MulDiv(52,dpi,96));
    SetWindowTextW(GetDlgItem(hwnd,NEW_PLAYLIST),TR(MUSIC_NEW_PLAYLIST));
    SetWindowTextW(GetDlgItem(hwnd,RENAME_PLAYLIST),TR(MUSIC_RENAME_PLAYLIST));
    SetWindowTextW(GetDlgItem(hwnd,DELETE_PLAYLIST),TR(MUSIC_DELETE_PLAYLIST));
    for (int i = LIST; i <= DELETE_PLAYLIST; ++i) SendDlgItemMessageW(hwnd,i,WM_SETFONT,(WPARAM)font,TRUE);
    LOGFONTW hint = {0};
    if (GetObjectW(font,sizeof(hint),&hint)) {
        hint.lfHeight = -MulDiv(11,dpi,96); hint.lfWeight = FW_NORMAL;
        HFONT hint_font = CreateFontIndirectW(&hint);
        if (hint_font) {
            SendDlgItemMessageW(hwnd,HINT,WM_SETFONT,(WPARAM)hint_font,TRUE);
            if (m->hint_font) DeleteObject(m->hint_font);
            m->hint_font = hint_font;
        }
    }
    refresh(hwnd,m);
    RECT rc; GetClientRect(hwnd,&rc); SendMessageW(hwnd,WM_SIZE,0,MAKELPARAM(rc.right,rc.bottom));
    InvalidateRect(hwnd,NULL,TRUE);
}

void music_reset(HWND hwnd) {
    Music *m = (Music *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if (!m) return;
    close_audio(m);
    for (int i = 1; i < m->playlist_count; ++i) free(m->playlists[i].paths);
    m->paths = m->playlists[0].paths;
    memset(m->playlists+1,0,sizeof(m->playlists)-sizeof(Playlist));
    m->playlist_count = 1; m->active_playlist = 0; m->playlists[0].count = 0;
    wcscpy(m->playlists[0].name,TR(MUSIC_DEFAULT_PLAYLIST));
    m->repeat_one = FALSE; m->count = 0; m->current = -1; m->load_failed = FALSE;
    refresh_playlists(hwnd,m);
    refresh(hwnd,m);
}
