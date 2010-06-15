#define COBJMACROS 1

#include <windows.h>
#include <winable.h>

static BOOL windows_enum_cb(HWND h_wnd, LPARAM param)
{
    (void) param;
    InvalidateRect(h_wnd, NULL, TRUE);
    UpdateWindow(h_wnd);
    
    return TRUE;
}

static BOOL refresh_desktop(void)
{
    INPUT input;    
    
    InvalidateRect(NULL, NULL, TRUE);
    InvalidateRect(GetDesktopWindow(), NULL, TRUE);
    UpdateWindow(GetDesktopWindow());
    
    EnumWindows(windows_enum_cb, 0);

    ZeroMemory(&input, sizeof input);
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_F5;
    input.ki.dwFlags = 0;
    SendInput(1U, &input, sizeof input);
    
    return TRUE;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    (void) hInstance;
    (void) hPrevInstance;
    (void) lpCmdLine;
    (void) nCmdShow;
    
    refresh_desktop();
    
    return 0;
}
