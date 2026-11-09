/* White-box checks use only a fresh .ui-check/memo-test-* directory. */
#include "../src/memo.c"
#include <assert.h>

static void text_fits(HDC dc, HFONT font, const wchar_t *text, int width, int height) {
    HGDIOBJ old = SelectObject(dc, font);
    RECT measured = {0,0,width,0};
    DrawTextW(dc,text,-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
    if (measured.right > width || measured.bottom > height)
        fwprintf(stderr,L"Text does not fit: %ls (%ld x %ld in %d x %d)\n",text,measured.right,measured.bottom,width,height);
    assert(measured.right <= width && measured.bottom <= height);
    SelectObject(dc,old);
}

int main(void) {
    wchar_t path[MAX_PATH];
    assert(GetModuleFileNameW(NULL,path,MAX_PATH));
    assert(wcsstr(path,L"\\.ui-check\\memo-test-"));
    wchar_t *slash = wcsrchr(path,L'\\'); assert(slash); wcscpy(slash+1,L"memos.dat");
    MemoBook book={0}, loaded={0};
    const wchar_t *note=L"重要会议 | planning\r\nBring C:\\notes\\agenda\n第二行";
    assert(memo_book_set(&book,"2024-02-29",note));
    assert(memo_book_set(&book,"2026-12-01",L"Birthday"));
    assert(!memo_book_set(&book,"2025-02-29",L"Invalid date"));
    assert(memo_book_save(&book,path));
    assert(memo_book_load(&loaded,path));
    assert(loaded.count==2 && !wcscmp(memo_book_get(&loaded,"2024-02-29"),note));
    assert(memo_book_set(&loaded,"2026-12-01",L"")); assert(loaded.count==1);
    wchar_t maximum[MEMO_TEXT_MAX];
    for(int i=0;i<MEMO_TEXT_MAX-1;++i) maximum[i]=L'记';
    maximum[MEMO_TEXT_MAX-1]=0;
    assert(memo_book_set(&loaded,"2026-12-01",maximum));
    assert(memo_book_save(&loaded,path)); assert(memo_book_load(&book,path));
    assert(!wcscmp(memo_book_get(&book,"2026-12-01"),maximum));
    FILE *bad=_wfopen(path,L"wb"); assert(bad); fputs("broken record\n",bad); fclose(bad);
    assert(!memo_book_load(&loaded,path)); assert(loaded.count==2);
    assert(memo_book_save(&book,path));
    INITCOMMONCONTROLSEX icc={sizeof(icc),ICC_DATE_CLASSES}; InitCommonControlsEx(&icc);
    HWND owner=CreateWindowExW(0,L"STATIC",L"Test",WS_POPUP,0,0,1200,1000,NULL,NULL,GetModuleHandleW(NULL),NULL);
    assert(owner);
    HWND hwnd=memo_create(owner,10); assert(hwnd);
    MemoView *v=view(hwnd); assert(!v->load_failed);
    SYSTEMTIME selected={0}; selected.wYear=2024;selected.wMonth=2;selected.wDay=29;
    assert(select_date(hwnd,selected));
    assert(GetWindowTextLengthW(v->edit)==(int)wcslen(note));
    SendMessageW(v->edit,EM_SETSEL,0,-1);
    SendMessageW(v->edit,EM_REPLACESEL,TRUE,(LPARAM)L"Updated offline plan"); assert(v->dirty);
    HANDLE lock=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(lock!=INVALID_HANDLE_VALUE);
    assert(!memo_flush(hwnd) && v->dirty);
    CloseHandle(lock); assert(memo_flush(hwnd) && !v->dirty);
    assert(memo_book_load(&loaded,path));
    assert(!wcscmp(memo_book_get(&loaded,"2024-02-29"),L"Updated offline plan"));
    assert(IsWindowEnabled(v->clear));
    lock=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(lock!=INVALID_HANDLE_VALUE);
    SendMessageW(hwnd,WM_COMMAND,5,0);
    assert(v->dirty && v->failed && GetWindowTextLengthW(v->edit)>0);
    assert(*memo_book_get(&v->book,"2024-02-29"));
    CloseHandle(lock);
    SendMessageW(hwnd,WM_COMMAND,5,0);
    assert(!v->dirty && !v->failed && !GetWindowTextLengthW(v->edit));
    assert(!IsWindowEnabled(v->clear));
    assert(memo_book_load(&loaded,path));
    assert(!*memo_book_get(&loaded,"2024-02-29"));
    assert(!wcscmp(memo_book_get(&loaded,"2026-12-01"),maximum));
    SendMessageW(hwnd,WM_COMMAND,3,0); assert(v->month==3);
    SendMessageW(hwnd,WM_COMMAND,2,0); assert(v->month==2);
    HDC dc=CreateCompatibleDC(NULL);
    for(int dpi=96;dpi<=192;dpi+=48) {
        int width=MulDiv(752,dpi,96), height=MulDiv(418,dpi,96);
        HFONT normal=CreateFontW(-MulDiv(16,dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
        HFONT small_font=CreateFontW(-MulDiv(13,dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
        SetWindowPos(hwnd,NULL,0,0,width,height,SWP_NOZORDER);
        for(int lang=0;lang<2;++lang) for(int theme=0;theme<3;++theme) {
            app_language=lang;app_theme_index=theme;memo_style(hwnd,dpi,normal,small_font);
            HBITMAP bmp=CreateBitmap(width,height,1,32,NULL); HGDIOBJ old=SelectObject(dc,bmp);
            memo_paint(hwnd,dc);
            int count=0;for(int i=0;i<42;++i) if(v->days[i]) ++count; assert(count==29);
            int paper_width=v->paper.right-v->paper.left-px(v,36);
            text_fits(dc,small_font,TR(MEMO_DAY_HINT),paper_width,px(v,28));
            text_fits(dc,small_font,TR(MEMO_SAVED),paper_width,px(v,48));
            text_fits(dc,small_font,TR(MEMO_SAVE_ERROR),paper_width,px(v,48));
            text_fits(dc,small_font,TR(MEMO_LOAD_ERROR),paper_width,px(v,48));
            text_fits(dc,normal,TR(MEMO_CLEAR),px(v,62),px(v,28));
            text_fits(dc,normal,L"2024 / 02 / 29",paper_width-px(v,84),px(v,28));
            RECT edit;GetWindowRect(v->edit,&edit);assert(edit.right>edit.left && edit.bottom>edit.top);
            SelectObject(dc,old);DeleteObject(bmp);
        }
        DeleteObject(normal);DeleteObject(small_font);
    }
    DeleteDC(dc);
    memo_reset(hwnd); assert(!v->dirty && v->book.count==0 && !GetWindowTextLengthW(v->edit));
    DestroyWindow(owner);
    memo_book_free(&book);memo_book_free(&loaded);
    assert(DeleteFileW(path));
    puts("PASS: Unicode/multiline/max-length memos, persistence, invalid files, save failure, calendar navigation, leap day, reset, 3 themes and 100/150/200% English/Chinese layout");
    return 0;
}
