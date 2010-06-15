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

const size_t size = 1024 * 1024 * 50;
const size_t chunk_size = 4096;
const unsigned int write_times = 3U;
const unsigned int read_times = 30U * 3U;
const char *file = "\\Evadeo\\evadeo-bench.tmp";

#define PC_TO_UL(PC) ((unsigned long long) (PC).LowPart + \
			  (unsigned long long) (PC).HighPart * 4294967296ULL)
#define TV_TO_TS(TV) (((double) (TV).tv_sec) + ((double) (TV).tv_usec) / 1000.0)

extern VOID ForcePageout(void);

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

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,
	    int nCmdShow)
{
    FILE *fp;
    unsigned int count;
    size_t written;
    char *junk;
    WCHAR strbuf[4096];
    WNDCLASS wnd_cls;
    HWND h_wnd;
    unsigned int progress = 0U;
    MSG msg;
    HWND h_test;
    time_t start1, end1, start2, end2;
    time_t duration_write, duration_read;
    unsigned int pct;
    
    ZeroMemory(&wnd_cls, sizeof wnd_cls);
    wnd_cls.style = CS_HREDRAW | CS_VREDRAW;
    wnd_cls.hInstance = hInstance;
    wnd_cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd_cls.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wnd_cls.lpszClassName = L"Evadeo Bench";
    wnd_cls.lpfnWndProc = (WNDPROC) wnd_proc;
    
    RegisterClass(&wnd_cls);
    h_wnd = FindWindow(L"Evadeo Bench", L"Evadeo Bench");
    if (h_wnd) {
	SetForegroundWindow((HWND) ((ULONG) h_wnd | 0x0001));
	return 0;
    }
    h_wnd = CreateWindow(L"Evadeo Bench", L"Evadeo Bench",
			 WS_VISIBLE, 0, 0,
			 CW_USEDEFAULT, CW_USEDEFAULT,
			 NULL, NULL, hInstance, NULL);
    if (!h_wnd) {
		MessageBox(NULL, L"Impossible de creer la fenetre",
		   TEXT("Evadeo Bench"), MB_OK);	
		return 1;
    }
    if (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
    }
    h_test = CreateWindow(WC_STATIC, L"Preparation du bench...",
			  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_EX_CONTROLPARENT,
			  0, 100, 480, 20, h_wnd, NULL, hInstance, NULL);

    ShowWindow(h_wnd, SW_SHOW);
    UpdateWindow(h_wnd);
    ForcePageout();
    if ((junk = malloc(chunk_size)) == NULL) {
		MessageBox(NULL, L"Plus assez de memoire disponible pour ce test",
		   TEXT("Evadeo Bench"), MB_OK);
		return 1;
    }
    memset(junk, 'A', chunk_size);
#if 0
    MessageBox(h_wnd,
	       L"Cliquez pour debuter le test de vitesse de la carte SD",
	       TEXT("Evadeo Bench, par Axey"), MB_OK);
#endif
    if ((fp = fopen(file, "wb")) == NULL) {
		unlink(file);
		free(junk);
		return 1;
    }    
    count = write_times;
    SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) L"Test en cours...");
    time(&start1);
    do {
	written = 0U;
	fseek(fp, 0L, SEEK_SET);
	do {
	    if (progress <= 0) {
		pct = written * 100 / size;
		_snwprintf(strbuf, sizeof strbuf,
			   L"Test d'ecriture (%u)-[%lu]",
			   count, (unsigned long) written);
		SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) strbuf);
		if (GetMessageW(&msg, NULL, 0, 0)) {
		    TranslateMessage(&msg);
		    DispatchMessageW(&msg);
		}
		progress = 250;
	    } else {
		progress--;
	    }
	    fwrite(junk, chunk_size, (size_t) 1U, fp);
	    fflush(fp);
	    written += chunk_size;
	} while (written < size);
	count--;	
    } while (count != 0);
    time(&end1);
    ForcePageout();
    count = read_times;
    time(&start2);
    do {
	written = 0U;
	fseek(fp, 0L, SEEK_SET);
	do {
	    if (progress <= 0) {
		pct = written * 100 / size;
		_snwprintf(strbuf, sizeof strbuf,
			   L"Test de lecture (%u)-[%lu]",
			   count, (unsigned long) written);
		SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) strbuf);
		if (GetMessageW(&msg, NULL, 0, 0)) {
		    TranslateMessage(&msg);
		    DispatchMessageW(&msg);
		}
		progress = 250 * 30;
	    } else {
		progress--;
	    }
	    fread(junk, chunk_size, (size_t) 1U, fp);
	    written += chunk_size;
	} while (written < size);
	count--;	
    } while (count != 0);
    time(&end2);
    fclose(fp);    
    unlink(file);
    free(junk);
    duration_write = end1 - start1;
    duration_read = end2 - start2;
    _snwprintf(strbuf, sizeof strbuf,
	       L"Ecriture : %lu sec\r\nLecture : %lu sec",
	       (unsigned long) duration_write,
	       (unsigned long) duration_read);
    MessageBox(h_wnd, strbuf,
	       L"Evadeo Bench par Axey pour www.evadeiste.fr", MB_OK);
    DestroyWindow(h_wnd);
    
#if 0
    while (GetMessageW(&msg, NULL, 0, 0)) {
	TranslateMessage(&msg);
	DispatchMessageW(&msg);
    }
#endif

    return 0;
}
