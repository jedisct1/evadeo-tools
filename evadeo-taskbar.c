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

static BOOL refresh_desktop(void)
{
    INPUT input;
    
    InvalidateRect(NULL, NULL, TRUE);
    InvalidateRect(GetDesktopWindow(), NULL, TRUE);
    
    ZeroMemory(&input, sizeof input);
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_F5;
    input.ki.dwFlags = 0;
    SendInput(1U, &input, sizeof input);
    
    return TRUE;
}

BOOL toggle_taskbar(void)
{
    HWND taskBarHWnd = FindWindow(L"HHTaskBar", L"");
    
    if (taskBarHWnd == NULL) {
	return FALSE;
    }
    if (IsWindowEnabled(taskBarHWnd)) {
	ShowWindow(taskBarHWnd, SW_HIDE);
	EnableWindow(taskBarHWnd, FALSE);
    } else {
	ShowWindow(taskBarHWnd, SW_SHOW);
	EnableWindow(taskBarHWnd, TRUE);	
    }
#if 0
    refresh_desktop();    
#endif
   
    return TRUE;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,
	    int nCmdShow)
{
    toggle_taskbar();
    
    return 0;
}
