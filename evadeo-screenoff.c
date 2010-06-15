#define COBJMACROS 1

#include <windows.h>

typedef void (*G407nVideoON)(WORD state);

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	(void) hInstance;
	(void) hPrevInstance;
	(void) lpCmdLine;
	(void) nCmdShow;

	HMODULE module;
	G407nVideoON g407_video_on;
	DWORD st = 1;
	HANDLE fh;
	static const WCHAR *state_file = L"\\Temp\\axey-screen-off.st";
	
	module = LoadLibraryEx(L"\\Evadeo\\evadeo\\G407nVDLL.dll", NULL, 0);	
	if (!module) {
		return 1;
	}
	g407_video_on = (G407nVideoON) GetProcAddress(module, L"G407nVideoON");
	if (g407_video_on) {
		if (GetFileAttributes(state_file) != INVALID_FILE_ATTRIBUTES) {
			DeleteFile(state_file);
		} else {
			fh = CreateFile(state_file, GENERIC_WRITE, FILE_SHARE_READ, 0,
				CREATE_NEW, FILE_ATTRIBUTE_HIDDEN, 0);
			if (fh) {
				CloseHandle(fh);
				st = 0;
			}
		}
		g407_video_on(st);
	}
	FreeLibrary(module);
	
	return 0;
}
