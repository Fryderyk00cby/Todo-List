#include "../src/music.c"
#include <assert.h>
int main(void) {
    HWND parent = CreateWindowW(L"STATIC", L"", WS_OVERLAPPED,0,0,800,600,NULL,NULL,NULL,NULL);
    HWND hwnd = music_create(parent, 900);
    assert(hwnd);
    music_style(hwnd,96,(HFONT)GetStockObject(DEFAULT_GUI_FONT));
    Music *m = (Music *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    assert(m && !m->load_failed);
    wchar_t directory[MAX_PATH]; GetModuleFileNameW(NULL,directory,MAX_PATH);
    *wcsrchr(directory,L'\\') = 0;
    wsprintfW(m->paths[0],L"%s\\first.wav",directory);
    wsprintfW(m->paths[1],L"%s\\second.wav",directory);
    m->count = 2;
    assert(save_playlist(m));
    play_track(hwnd,m,0,1);
    assert(m->device && m->current == 0);
    SendMessageW(hwnd,WM_COMMAND,PLAY,0); assert(m->paused);
    SendMessageW(hwnd,WM_COMMAND,PLAY,0); assert(!m->paused);
    SendMessageW(hwnd,MM_MCINOTIFY,MCI_NOTIFY_SUCCESSFUL,m->device);
    assert(m->device && m->current == 1);
    SendMessageW(hwnd,MM_MCINOTIFY,MCI_NOTIFY_SUCCESSFUL,m->device);
    assert(m->device && m->current == 0);
    DestroyWindow(hwnd);
    hwnd = music_create(parent,900);
    m = (Music *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    assert(m->count == 2 && m->current == -1 && !m->device);
    music_style(hwnd,144,(HFONT)GetStockObject(DEFAULT_GUI_FONT));
    SendDlgItemMessageW(hwnd,LIST,LB_SETCURSEL,0,0);
    SendMessageW(hwnd,WM_COMMAND,REMOVE,0); assert(m->count == 1);
    wcscpy(m->paths[0],L"Z:\\missing.wav");
    play_track(hwnd,m,0,1); assert(!m->device);
    music_reset(hwnd); assert(!m->count && !m->device);
    DestroyWindow(parent);
    puts("Music tests passed (WAV playback, pause/resume, playlist loop, persistence, removal, missing file, reset).");
    return 0;
}
