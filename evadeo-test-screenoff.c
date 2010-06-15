#include <windows.h>

typedef void (*G407nVideoON)(BOOL state);

static BOOL set_backlight(const BOOL state)
{
	HMODULE module;
	G407nVideoON g407_video_on;

	module = LoadLibraryEx(L"\\Evadeo\\evadeo\\G407nVDLL.dll", NULL, 0);
	if (module == NULL) {
		return FALSE;
	}
	g407_video_on = (G407nVideoON) GetProcAddress(module, L"G407nVideoON");
	if (g407_video_on == NULL) {
		FreeLibrary(module);
		return FALSE;
	}
	g407_video_on(state);
	FreeLibrary(module);
	
	return TRUE;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	MEMORYSTATUS mem_status1, mem_status2;
	unsigned int c = 1000U;
	FILE *fp;
	
	GlobalMemoryStatus(&mem_status1);
	do {
		set_backlight(FALSE);
		set_backlight(TRUE);
	} while (--c != 0);
	GlobalMemoryStatus(&mem_status2);
	if ((fp = _wfopen(L"\\Evadeo\\test.txt", L"wt")) != NULL) {
		fprintf(fp, "Avant : [%lu] Apres : [%lu]\n",
			(unsigned long) mem_status1.dwAvailPhys,
			(unsigned long) mem_status2.dwAvailPhys);
		fclose(fp);
	}	
	return 0;
}
