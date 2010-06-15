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

extern BOOL GetSystemMemoryDivision(LPDWORD lpdwStorePages,
				    LPDWORD lpdwRamPages,
				    LPDWORD lpdwPageSize);
extern DWORD SetSystemMemoryDivision(DWORD dwStorePages);
extern VOID ForcePageout(void);

#ifndef SYSMEM_CHANGED
# define SYSMEM_CHANGED   0
# define SYSMEM_MUSTREBOOT 1
# define SYSMEM_REBOOTPENDING 2
# define SYSMEM_FAILED 3
#endif

BOOL refresh_desktop(void)
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

BOOL rotate(void)
{
    DEVMODE dev_mode;
    
    ZeroMemory(&dev_mode, sizeof dev_mode);
    dev_mode.dmSize = sizeof dev_mode;
    dev_mode.dmFields = DM_DISPLAYQUERYORIENTATION;
    if (ChangeDisplaySettingsEx(NULL, &dev_mode, NULL, CDS_TEST, NULL)
	!= DISP_CHANGE_SUCCESSFUL) {
	MessageBox(0,
		   L"L'ecran de votre Evadeo ne peut pas etre tourne",
		   L"Erreur", MB_OK);
	return FALSE;
    }
    dev_mode.dmFields = DM_DISPLAYORIENTATION;
    ChangeDisplaySettingsEx(NULL, &dev_mode, NULL, CDS_TEST, NULL);
    dev_mode.dmSize = sizeof dev_mode;
    switch (dev_mode.dmDisplayOrientation) {
    case DMDO_0:	
	dev_mode.dmDisplayOrientation = DMDO_90;
	break;
    case DMDO_90:
	dev_mode.dmDisplayOrientation = DMDO_0;
	break;
    case DMDO_270:
	dev_mode.dmDisplayOrientation = DMDO_180;	
	break;
    case DMDO_180:
	dev_mode.dmDisplayOrientation = DMDO_270;
	break;
    default:
	dev_mode.dmDisplayOrientation = DMDO_0;
    }
    ChangeDisplaySettingsEx(NULL, &dev_mode, NULL, CDS_RESET, NULL);
    refresh_desktop();
    
    return TRUE;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,
	    int nCmdShow)
{
    rotate();
    
    return 0;
}
