#define COBJMACROS 1

#include <windows.h>
#include <winable.h>
#include <ntdef.h>
#include <winreg.h>
#include <shellapi.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#define FRAME_RATE (1000 / 25)
#define USE_SETDIBBITS 0

#define QUERYESCSUPPORT    8

// The following are unique to CE
#define GETVFRAMEPHYSICAL   6144
#define GETVFRAMELEN    6145
#define DBGDRIVERSTAT    6146
#define SETPOWERMANAGEMENT   6147
#define GETPOWERMANAGEMENT   6148

typedef enum _VIDEO_POWER_STATE {
	VideoPowerOn = 1,
	VideoPowerStandBy,
	VideoPowerSuspend,
	VideoPowerOff
} VIDEO_POWER_STATE, *PVIDEO_POWER_STATE;

typedef struct _VIDEO_POWER_MANAGEMENT {
	ULONG Length;
	ULONG DPMSVersion;
	ULONG PowerState;
} VIDEO_POWER_MANAGEMENT, *PVIDEO_POWER_MANAGEMENT;

typedef struct _VIDEOPARAMETERS {
  GUID Guid;
  ULONG dwOffset;
  ULONG dwCommand;
  ULONG dwFlags;
  ULONG dwMode;
  ULONG dwTVStandard;
  ULONG dwAvailableModes;
  ULONG dwAvailableTVStandard;
  ULONG dwFlickerFilter;
  ULONG dwOverScanX;
  ULONG dwOverScanY;
  ULONG dwMaxUnscaledX;
  ULONG dwMaxUnscaledY;
  ULONG dwPositionX;
  ULONG dwPositionY;
  ULONG dwBrightness;
  ULONG dwContrast;
  ULONG dwCPType;
  ULONG dwCPCommand;
  ULONG dwCPStandard;
  ULONG dwCPKey;
  ULONG bCP_APSTriggerBits;
  UCHAR bOEMCopyProtection[256];
} VIDEOPARAMETERS, *PVIDEOPARAMETERS, *LPVIDEOPARAMETERS;

#define CDS_VIDEOPARAMETERS 0x00000020
#define VP_FLAGS_FLICKER 0x00000004 
#define VP_COMMAND_GET 0x00000001 
#define VP_COMMAND_SET 0x00000002 

static BOOL screen_off(void)
{
	HDC gdc;
	int iESC=SETPOWERMANAGEMENT;

	gdc = GetDC(NULL);
	if (ExtEscape(gdc, QUERYESCSUPPORT, sizeof(int), (LPCSTR) &iESC, 0, NULL) == 0) {
		MessageBox(NULL, L"Sorry, your Pocket PC does not support DisplayOff",
			             L"Pocket PC Display Off Feature", MB_OK);
	} else {
		VIDEO_POWER_MANAGEMENT vpm;
		vpm.Length = sizeof(VIDEO_POWER_MANAGEMENT);
		vpm.DPMSVersion = 0x0001;
		vpm.PowerState = VideoPowerOff;
	// Power off the display
		ExtEscape(gdc, SETPOWERMANAGEMENT, vpm.Length, (LPCSTR) &vpm, 
			0, NULL);
		Sleep(5000);
		vpm.PowerState = VideoPowerOn;
	// Power on the display
		ExtEscape(gdc, SETPOWERMANAGEMENT, vpm.Length, (LPCSTR) &vpm, 
			0, NULL);
		ReleaseDC(NULL, gdc);
	}
	return TRUE;
}

/*
1. SelectObject(oldBmp) for SelectObject(compatibleBmp)
2. DeleteDC() for CreateCompatibleDC()
3. DeleteObject() for CreateCompatibleBitmap()
*/

LRESULT wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static WORD width, height;
	static HDC hdc;
	static BITMAPINFO bmi;
	static HDC bmap_dc;
	static HBITMAP hbmap;
	static HBITMAP hbmap_dib;	
	static unsigned char *ppvbits;
	static unsigned char *bigbuf;
	static UINT timer;
	static int *track;
	static int *track_end;
	RECT rect;
	PAINTSTRUCT ps;	
		
	switch(msg){ 
		case WM_CREATE:
		hdc = GetDC(hWnd);
		bmap_dc = CreateCompatibleDC(hdc);
		GetClientRect(hWnd, &rect);
		width = rect.right;
		height = rect.bottom;
		ZeroMemory(&bmi, sizeof bmi);		
		hbmap = CreateCompatibleBitmap(bmap_dc, width, height);
		bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
		bmi.bmiHeader.biWidth = width;
		bmi.bmiHeader.biHeight = height;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biBitCount = 24;
		bmi.bmiHeader.biSizeImage =
			bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * bmi.bmiHeader.biBitCount / 8;
		bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
		hbmap_dib = CreateDIBSection(bmap_dc, &bmi, DIB_RGB_COLORS, (void *) &ppvbits, NULL, 0);
		SelectObject(bmap_dc, hbmap_dib);		
		BitBlt(bmap_dc, 0, 0, width, height, NULL, 0, 0, WHITENESS);
		BitBlt(hdc, 0, 0, width, height, NULL, 0, 0, WHITENESS);	
		
		{
			FILE *fp;
			
			fp = _wfopen(L"\\Evadeo\\Extra\\Sys\\Media\\ACCUEIL.BMP", L"rb");
			fseek(fp, 54L, SEEK_SET);
			fread(ppvbits, width * height * 3, 1, fp);
			fclose(fp);
			
		}	
		bigbuf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, width * height * 3 * 3);
		{
			int y = 0;
			unsigned char *t;
			
			do {
				memcpy(bigbuf + width * 3 * 3 * y + width * 3, ppvbits + width * 3 * y,
					width * 3);
#if 1					
				t = bigbuf + width * 3 * 3 * y;
				memset(t, 255, width * 3);
				t = bigbuf + width * 3 * 3 * y + width * 3 * 2;
				memset(t, 255, width * 3);
#else
				memcpy(bigbuf + width * 3 * 3 * y, bigbuf + width * 3 * 3 * y + width * 3,
					width * 3);
				memcpy(bigbuf + width * 3 * 3 * y + width * 3 * 2, bigbuf + width * 3 * 3 * y + width * 3,
					width * 3);
#endif					
			} while (++y < height);
		}
		{
			int *tp;
			int a;
			double wideness;
			double g = 0.0;
			
			track = HeapAlloc(GetProcessHeap(), 0, 10000 * sizeof *track);
			tp = track;
			a = height;
			do {
				break;
				a--;
				*tp++ = - (width - 1) * 3;
			} while (a > 0);
			a = 0;
			do {
				wideness = (width - 1) * sin(M_PI_2 + M_PI_2 * ((double) a) / 100.0);
				*tp++ = round(wideness * sin(g)) * 3;
				*tp++ = -round(wideness * sin(g)) * 3;
				*tp++ = round(wideness * cos(g * 2.0)) * 3;
				*tp++ = -round(wideness * cos(g * 2.0)) * 3;
				g += 0.02;
			} while (a++ < 100);
			track_end = tp;	
			a = height + 1;		
			do {
				a--;
				*tp++ = 0;
			} while (a != 0);
		}				
		return 1;
		
		case WM_CLOSE:
		DestroyWindow(hWnd);
		
		return 1;
		
		case WM_ERASEBKGND:
		return 1;
		
		case WM_DESTROY:
		DeleteDC(bmap_dc);
		DeleteObject(hbmap_dib);
		DeleteObject(hbmap);		
		ReleaseDC(hWnd, hdc);
		if (bigbuf != NULL) {
			HeapFree(GetProcessHeap(), 0, bigbuf);
		}
		if (track != NULL) {
			HeapFree(GetProcessHeap(), 0, track);
		}
		PostQuitMessage(0);
		
		return 1;
		
		case WM_TIMER:
		timer = SetTimer(hWnd, 1, FRAME_RATE, NULL);		
		InvalidateRect(hWnd, NULL, FALSE);	
			
		return 1;
	}
	if (msg == WM_PAINT) {
		hdc = BeginPaint(hWnd, &ps);
		
		{
			int y = 0;
			static int *track_base = NULL;
			static BOOL last = FALSE;
			unsigned char *src, *dst;
			int *track_pnt;
			int off;	
			
			if (track_base == NULL) {
				track_base = track;
			}
			track_pnt = track_base;
			do {
				dst = ppvbits + width * y * 3;
				off = *track_pnt;
				if (off < width * -3 || off > width * 3) {
					off = 0;
				}
				src = bigbuf + width * 3 * 3 * y + width * 3 + off;
				track_pnt++;				
				memcpy(dst, src, width * 3);
			} while (++y < height);
			if (last == FALSE) {
				y = 8;
				do {
					track_base++;
					if (track_base == track_end) {
						last = TRUE;
						break;
					}
				} while (--y > 0);
			}
		}
#if USE_SETDIBBITS == 0
		BitBlt(hdc, 0, 0, width, height, bmap_dc, 0, 0, SRCCOPY);
#else		
		SetDIBitsToDevice(hdc, 0, 0, width, height, 0, 0, 0, height,
						  ppvbits, &bmi, DIB_RGB_COLORS);
#endif		
		EndPaint(hWnd, &ps);

		if (timer == 0) {
			timer = SetTimer(hWnd, 1, FRAME_RATE, NULL);
		}
		return 1;
	}
	if (msg == 1360)
	{
		WCHAR str[1000];
		
		_snwprintf(str, sizeof str, L"%d", msg);
		MessageBox(NULL, str, str, MB_OK);
	}
	return DefWindowProc(hWnd, msg, wParam, lParam); 
}

static void try(void)
{
	HKL hkls[100];
	WCHAR name[KL_NAMELENGTH];
	unsigned int n;
	
	n = GetKeyboardLayoutList(100, hkls);
	while (n > 0) {
		ActivateKeyboardLayout(hkls[n], 0);
		GetKeyboardLayoutName(name);
		MessageBox(NULL, name, L"OK", MB_OK);
		n--;
	}
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,	int nCmdShow)
{
	WNDCLASS wnd_cls;
	HWND h_wnd;
	MSG msg;

	(void) hPrevInstance;
	(void) lpCmdLine;
	(void) nCmdShow;
	(void) screen_off;

	{
		DEVMODE devmode;
		VIDEOPARAMETERS vp;
		
		ZeroMemory(&vp, sizeof vp);
		vp.dwCommand = VP_COMMAND_GET;
		ChangeDisplaySettingsEx(NULL, &devmode, NULL, CDS_VIDEOPARAMETERS, &vp);
		vp.dwCommand = VP_COMMAND_SET;
		vp.dwFlickerFilter = 1000;
		vp.dwFlags = VP_FLAGS_FLICKER;
		ChangeDisplaySettingsEx(NULL, &devmode, NULL, CDS_VIDEOPARAMETERS | CDS_UPDATEREGISTRY, &vp);
	}
	
	ZeroMemory(&wnd_cls, sizeof wnd_cls);
	wnd_cls.style = CS_HREDRAW | CS_VREDRAW;
	wnd_cls.hInstance = hInstance;
	wnd_cls.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd_cls.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
	wnd_cls.lpszClassName = L"Evadeo Splash";
	wnd_cls.lpfnWndProc = (WNDPROC) wnd_proc;	

	RegisterClass(&wnd_cls);
	h_wnd = FindWindow(L"Evadeo Splash", L"Evadeo Splash");
	if (h_wnd) {
		SetForegroundWindow((HWND) ((ULONG) h_wnd | 0x0001));
		return 0;
	}
	h_wnd = CreateWindow(L"Evadeo Splash", L"Evadeo Splash",
		WS_VISIBLE, 0, 0,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL);
	if (h_wnd == NULL) {
		return 1;
	}
	ShowWindow(h_wnd, SW_SHOW);
	UpdateWindow(h_wnd);
	while (GetMessage(&msg, NULL, 0, 0)) {
      	TranslateMessage(&msg);
      	DispatchMessage(&msg);
   	}
	DestroyWindow(h_wnd);
	UnregisterClass(wnd_cls.lpszClassName, NULL);

   	return msg.wParam;
}
