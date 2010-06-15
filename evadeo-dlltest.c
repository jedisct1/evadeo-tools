#define COBJMACROS 1

#include <windows.h>
#include <winable.h>
#include <ntdef.h>
#include <winreg.h>
#include <shellapi.h>
#include <commctrl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <shlwapi.h>

extern VOID ForcePageout(void);

static HWND h_test;
const WCHAR *log_file = L"\\Evadeo\\evadeo-dlltest.txt";

LRESULT wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg){ 
    case WM_CREATE: 
        return 1;
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default: 
        return DefWindowProc(hWnd,msg,wParam,lParam); 
    } 
    return 0; 
}

static const WCHAR *pe_analyze(const WCHAR *src_path)
{
    static WCHAR strbuf[4000];
    int c;
    FILE *fp;
    WORD word;
    LONG u32;
    LONG pe_offset;
    time_t ts;
    WORD characteristics;
    BYTE linker_major;
    BYTE linker_minor;
    BYTE osreq_major;
    BYTE osreq_minor;
    BYTE oscomp_major;
    BYTE oscomp_minor;  
    const WCHAR *arch = L"architecture inconnue";
    struct tm *ttm;
    
    if ((fp = _wfopen(src_path, L"rb")) == NULL) {
        return L"* Impossible d'acceder au fichier *";
    }
    c = fgetc(fp);
    if (c == EOF) {
        fclose(fp);
        return L"* Analyse impossible - Fichier protege ? (EOF) *";
    }
    word = (WORD) c;
    word |= (fgetc(fp) << 8);   
    if (word == 0 || word == 0xffff) {
        fclose(fp);
        _snwprintf(strbuf, sizeof strbuf, L"* Analyse impossible - Fichier protege ? (%04x) *", word);
        return strbuf;
    }
    if (word != IMAGE_DOS_SIGNATURE) {
        fclose(fp);
        _snwprintf(strbuf, sizeof strbuf, L"* Mauvaise signature historique (%04x) *", word);
        return strbuf;
    }
    if (fseek(fp, 0x3C, SEEK_SET) != 0) {
        fclose(fp);
        return L"* Position PE impossible a atteindre *";
    }
    pe_offset = fgetc(fp);
    pe_offset |= (fgetc(fp) << 8);
    pe_offset |= (fgetc(fp) << 16);
    pe_offset |= (fgetc(fp) << 24);
    if (pe_offset < 0x3C) {
        fclose(fp);
        return L"* Position PE invalide *";
    }
    if (fseek(fp, pe_offset, SEEK_SET) != 0) {
        fclose(fp);
        return L"* Signature PE impossible a atteindre *";
    }
    u32 = fgetc(fp);
    u32 |= (fgetc(fp) << 8);
    u32 |= (fgetc(fp) << 16);
    u32 |= (fgetc(fp) << 24);
    if (u32 != IMAGE_NT_SIGNATURE) {
        fclose(fp);
        return L"* Signature PE invalide *";
    }
    word = fgetc(fp);
    word |= (fgetc(fp) << 8);
    switch (word) {
    case IMAGE_FILE_MACHINE_ARM:
        arch = L"ARM";
        break;
    case IMAGE_FILE_MACHINE_THUMB:
        arch = L"ARM (thumb)";
        break;
    case IMAGE_FILE_MACHINE_I386:
    case IMAGE_FILE_MACHINE_AMD64:
        fclose(fp);
        return L"* Architecture incompatible (fichier pour PC) *";
    default:
        fclose(fp);
        _snwprintf(strbuf, sizeof strbuf, L"* Architecture incompatible (%x) *",
                   (unsigned int) word);
        return strbuf;
    }
    word = fgetc(fp);
    word |= (fgetc(fp) << 8);
    if (word <= 0)       {
        fclose(fp);
        return L"* Aucune section *";
    }
    ts = fgetc(fp);
    ts |= (fgetc(fp) << 8);
    ts |= (fgetc(fp) << 16);
    ts |= (fgetc(fp) << 24);
    if (fseek(fp, 8, SEEK_CUR) != 0) {
        fclose(fp);
        return L"* En-tête COFF incomplet *";
    }
    word = fgetc(fp);
    word |= (fgetc(fp) << 8);
    characteristics = fgetc(fp);
    characteristics |= (fgetc(fp) << 8);
    (void) characteristics;
    if (word > 0) {
        word = fgetc(fp);
        word |= (fgetc(fp) << 8);
        if (word != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
            word != IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
            word != IMAGE_ROM_OPTIONAL_HDR_MAGIC) {
            fclose(fp);
            return L"* Mauvaise signature pour l'en-tête facultatif *";                        
        }
    }
    linker_major = fgetc(fp);
    linker_minor = fgetc(fp);
    if (fseek(fp, 36, SEEK_CUR) != 0) {
        fclose(fp);
        return L"* MajorOperatingSystemVersion impossible a atteindre *";
    }
    osreq_major = fgetc(fp);
    osreq_major |= (fgetc(fp) << 8);
    osreq_minor = fgetc(fp);
    osreq_minor |= (fgetc(fp) << 8);    
    fgetc(fp);  fgetc(fp);
    fgetc(fp);  fgetc(fp);      
    oscomp_major = fgetc(fp);
    oscomp_major |= (fgetc(fp) << 8);
    oscomp_minor = fgetc(fp);
    oscomp_minor |= (fgetc(fp) << 8);   
    if (fseek(fp, 16, SEEK_CUR) != 0) {
        fclose(fp);
        return L"* Subsystem impossible a atteindre *";
    }
    word = fgetc(fp);
    word |= (fgetc(fp) << 8);
    if (word != IMAGE_SUBSYSTEM_WINDOWS_CE_GUI) {
        fclose(fp);
        return L"* Code incompatible avec Windows CE *";                
    }
    fclose(fp);
    ttm = localtime(&ts);
    
    _snwprintf(strbuf, sizeof strbuf, L"OK - Date : %02d.%02d.%d - "
               "Compile pour Windows CE %u.%u - Compatible avec Windows CE >= %u.%u - "
               "Linker version %u.%u",
               ttm->tm_mday, ttm->tm_mon + 1, ttm->tm_year + 1900,
               (unsigned) oscomp_major, (unsigned) oscomp_minor,
               (unsigned) osreq_major, (unsigned) osreq_minor,
               (unsigned) linker_major, (unsigned) linker_minor
              );        
    return strbuf;
}

static BOOL check_dlls(const LPWSTR base_path, const LPWSTR mask, const BOOL try_run,
                       FILE * const log_fp)
{
    HANDLE fh;
    WIN32_FIND_DATA found;
    LPWSTR file_name;
    WCHAR full_path[MAX_PATH];
    WCHAR src_path[MAX_PATH];
    CHAR src_path_a[MAX_PATH];  
    WCHAR strbuf[4000];
    ULONG size;
    HMODULE module;
    MSG msg;
    const WCHAR *pe_ret;
    CHAR pe_ret_a[4000];
    WCHAR loadlib_err[250];
    
    _snwprintf(full_path, sizeof full_path, L"%s\\%s", base_path, mask);
    fh = FindFirstFile(full_path, &found);
    if (fh == INVALID_HANDLE_VALUE) {
        FindClose(fh);
        return FALSE;
    }
    do {
        size = found.nFileSizeHigh * (MAXDWORD + 1) + found.nFileSizeLow;
        file_name = found.cFileName;
        _snwprintf(src_path, sizeof src_path, L"%s\\%s",
                   base_path, file_name);
        SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) src_path);
        if (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        pe_ret = pe_analyze(src_path);
        if (try_run == TRUE && *pe_ret != L'*') {
            module = LoadLibraryEx(src_path, NULL, 0);
            if (module == NULL) {
                DWORD dw = GetLastError();
                LPVOID lp_msg_buf;                                
                FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                              FORMAT_MESSAGE_FROM_SYSTEM |
                              FORMAT_MESSAGE_IGNORE_INSERTS,
                              NULL,
                              dw,
                              MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              (LPTSTR) &lp_msg_buf,
                              0, NULL);
                _snwprintf(loadlib_err, sizeof loadlib_err,
                           L"* Fichier fonctionnellement incompatible avec cet appareil (%s) *",
                           lp_msg_buf);
                LocalFree(lp_msg_buf);
                pe_ret = loadlib_err;
            } else {
                FreeLibrary(module);
            }
        }
        if (*pe_ret == L'*') {
            _snwprintf(strbuf, sizeof strbuf, L"Fichier [%s] : [%s]",
                       src_path, pe_ret);
#if 1                           
            SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) strbuf);
            if (GetMessageW(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            Sleep(250);
#else                   
            MessageBox(NULL, strbuf, L"Fichier incompatible", MB_OK);
#endif                  
        }
        if (log_fp != NULL) {
            WideCharToMultiByte(CP_ACP, 0, src_path, -1,
                                src_path_a, sizeof src_path_a, NULL, NULL);
            WideCharToMultiByte(CP_ACP, 0, pe_ret, -1,
                                pe_ret_a, sizeof pe_ret_a, NULL, NULL);
            fprintf(log_fp, "%s :\t%s\n\n", src_path_a, pe_ret_a);
        }
    } while (FindNextFile(fh, &found) == TRUE);
    FindClose(fh);
    
    return TRUE;        
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,
            int nCmdShow)
{   
    WCHAR strbuf[1000];
    WNDCLASS wnd_cls;
    HWND h_wnd;
    MSG msg;
    FILE *log_fp = NULL;
    
    (void) hPrevInstance;
    (void) lpCmdLine;
    (void) nCmdShow;
    
    ZeroMemory(&wnd_cls, sizeof wnd_cls);
    wnd_cls.style = CS_HREDRAW | CS_VREDRAW;
    wnd_cls.hInstance = hInstance;
    wnd_cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd_cls.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wnd_cls.lpszClassName = L"Evadeo DLLTest";
    wnd_cls.lpfnWndProc = (WNDPROC) wnd_proc;
    
    RegisterClass(&wnd_cls);
    h_wnd = FindWindow(L"Evadeo DLLTest", L"Evadeo DLLTest");
    if (h_wnd) {
        SetForegroundWindow((HWND) ((ULONG) h_wnd | 0x0001));
        return 0;
    }
    h_wnd = CreateWindow(L"Evadeo DLLTest", L"Evadeo DLLTest",
                         WS_VISIBLE, 0, 0,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         NULL, NULL, hInstance, NULL);
    if (!h_wnd) {
        MessageBox(NULL, L"Impossible de creer la fenetre",
                   TEXT("Evadeo DLLTest"), MB_OK);      
        return 1;
    }
    if (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    h_test = CreateWindow(WC_STATIC, L"Evadeo DLLTest",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_EX_CONTROLPARENT,
                          0, 100, 480, 40, h_wnd, NULL, hInstance, NULL);
    ShowWindow(h_wnd, SW_SHOW);
    UpdateWindow(h_wnd);
    ForcePageout();
    SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) L"Test de compatibilite en cours...");
    if (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if ((log_fp = _wfopen(log_file, L"wt")) == NULL) {
        _snwprintf(strbuf, sizeof strbuf, L"Impossible de creer le fichier [%s]", log_file);
        MessageBox(NULL, strbuf, L"Erreur", MB_OK);
    }
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs", L"*.DLL", TRUE, log_fp);
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs2", L"*.DLL", TRUE, log_fp); 
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs3", L"*.DLL", TRUE, log_fp);
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs-DOTNET", L"*.DLL", TRUE, log_fp);
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs-Test", L"*.DLL", TRUE, log_fp);
    
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs", L"*.EXE", FALSE, log_fp);
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs2", L"*.EXE", FALSE, log_fp);        
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs3", L"*.EXE", FALSE, log_fp);
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs-DOTNET", L"*.EXE", FALSE, log_fp);
    check_dlls(L"\\Evadeo\\Extra\\Sys\\DLLs-Test", L"*.EXE", FALSE, log_fp);
    
    check_dlls(L"\\Evadeo\\Extra\\Sys\\EXEs", L"*.EXE", FALSE, log_fp);
    check_dlls(L"\\Evadeo\\Extra\\Sys\\EXEs2", L"*.EXE", FALSE, log_fp);        
    check_dlls(L"\\Evadeo\\Extra\\Sys\\EXEs3", L"*.EXE", FALSE, log_fp);
    check_dlls(L"\\Evadeo\\Extra\\Sys\\EXEs-DOTNET", L"*.EXE", FALSE, log_fp);
    check_dlls(L"\\Evadeo\\Extra\\Sys\\EXEs-Test", L"*.EXE", FALSE, log_fp);
    
    if (log_fp != NULL) {
        fclose(log_fp);
        _snwprintf(strbuf, sizeof strbuf, L"Test termine - "
                   "Vous trouverez un rapport complet ici : [%s]", log_file);
        MessageBox(NULL, strbuf,
                   TEXT("DLLTest par Axey"), MB_OK);
    }
    DestroyWindow(h_wnd);
    
    return 0;
}
