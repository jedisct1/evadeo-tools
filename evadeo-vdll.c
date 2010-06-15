#include <windows.h>

typedef BOOL (*G407nVideoON_t)(BOOL state);

BOOL G407nVideoON(const BOOL state)
{
	HMODULE module;
	G407nVideoON_t g407_video_on;

	module = LoadLibraryEx(L"\\Evadeo\\evadeo\\G407nVDLL.dll-old", NULL, 0);
	if (module == NULL) {
		return FALSE;
	}
	g407_video_on = (G407nVideoON_t) GetProcAddress(module, L"G407nVideoON");
	if (g407_video_on == NULL) {
		FreeLibrary(module);
		return FALSE;
	}
	g407_video_on(state);
	Sleep(500);
	g407_video_on(state);	
	Sleep(500);
	g407_video_on(state);	
	FreeLibrary(module);
	
	return TRUE;
}
