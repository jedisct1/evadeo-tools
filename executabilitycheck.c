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

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,
	    int nCmdShow)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    LPWSTR startup;
    
    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    ZeroMemory(&pi, sizeof pi);
    
    startup = L"\\Evadeo\\Extra\\Sys\\Wrapped\\executabilitycheck.wrp";
    if (!CreateProcess(startup, L"\\Evadeo", // /e
		       NULL, NULL, FALSE, 0,
		       NULL, NULL, &si, &pi)) {
	WCHAR io[1000];
	
	_snwprintf(io, sizeof io, L"Big Foirage [%d]\n", (int) GetLastError());
	MessageBox(NULL, io, TEXT("Axey says..."), MB_OK);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);    
    
    return 0;
}
