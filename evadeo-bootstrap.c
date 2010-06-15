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
#include <string.h>
#include <ctype.h>
#include <math.h>

extern BOOL GetSystemMemoryDivision(LPDWORD lpdwStorePages,
                                    LPDWORD lpdwRamPages,
                                    LPDWORD lpdwPageSize);
extern DWORD SetSystemMemoryDivision(DWORD dwStorePages);
extern VOID ForcePageout(void);
extern void FileSystemPowerFunction(DWORD dwflags);
extern void PowerOffSystem(void);

#ifndef SYSMEM_CHANGED
# define SYSMEM_CHANGED   0
# define SYSMEM_MUSTREBOOT 1
# define SYSMEM_REBOOTPENDING 2
# define SYSMEM_FAILED 3
#endif

#ifndef FSNOTIFY_POWER_ON
# define FSNOTIFY_POWER_ON  0x00000001
# define FSNOTIFY_POWER_OFF 0x00000002
#endif

#define FRAME_RATE (1000 / 25)
#define USE_SETDIBBITS 0
#define MORTSCRIPT_TIMEOUT (1000 * 10)
#define AUTOSTART_TIMEOUT (1000 * 10)

typedef struct ShortCut_ {
	WCHAR *link;
	WCHAR *target;
	struct ShortCut_ *next;
} ShortCut;

typedef struct AutoStart_ {
	WCHAR *file;
	struct AutoStart_ *next;
} AutoStart;

typedef struct MortScript_ {
	WCHAR *file;
	struct MortScript_ *next;
} MortScript;

struct YMLParserContext_;

typedef int (*YMLParserSectionHandler)(struct YMLParserContext_ *yml_parser_context, WCHAR *line);

typedef struct YMLParserSection_ {
	const WCHAR *section_name;
	YMLParserSectionHandler handler;	
} YMLParserSection;

typedef struct YMLParserContext_ {
	YMLParserSection *current_section;
	ShortCut *shortcuts_first, *shortcuts_last;
	WCHAR *background;
	BOOL is_explorer;
	int backlight;
	int volume;
	BOOL delete_temporary_files;
	BOOL disable_usbfn;
	BOOL enable_boost;	
	AutoStart *autostarts_first, *autostarts_last;
	MortScript *mortscripts_first, *mortscripts_last;
} YMLParserContext;

typedef struct YMLParserStrPair_ {
	WCHAR *str1;
	WCHAR *str2;
} YMLParserStrPair;

static WCHAR *skip_spaces(const WCHAR *str)
{
	while (iswspace(*str)) {
		str++;
	}
	return (WCHAR *) str;
}

static WCHAR *chomp(WCHAR *str)
{
	WCHAR *pnt;
	
	if ((pnt = wcsrchr(str, L'\n')) != NULL) {
		*pnt = 0;
	}
	if ((pnt = wcsrchr(str, L'\r')) != NULL) {
		*pnt = 0;
	}
	return str;
}

static BOOL create_link(LPWSTR target, LPWSTR link, LPWSTR desc)
{
	BOOL res;
    
	(void) desc;
	if (GetFileAttributes(target) == INVALID_FILE_ATTRIBUTES) {
		return FALSE;
	}
	res = SHCreateShortcut(link, target);
    
	return res;
}

static BOOL registry_delete_value(HKEY domain, LPWSTR path, LPWSTR key)
{
	BOOL ret = TRUE;
	HKEY hk = NULL;

	if (RegOpenKeyEx(domain, path, 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
		if (RegDeleteValue(hk, key) != ERROR_SUCCESS) {
			ret = FALSE;
		}
		if (RegCloseKey(hk)) {
			return FALSE;
		}
	}
	return ret;
}

static BOOL registry_write_dword(HKEY domain, LPWSTR path, LPWSTR key, const DWORD value)
{
	HKEY hk = NULL;
    
	if (RegOpenKeyEx(domain, path, 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
		if (RegSetValueEx(hk, key, 0, REG_DWORD, (const BYTE *) &value, sizeof value)) {
            /* SNIF... */
		}
		if (RegCloseKey(hk)) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL registry_create_dword(HKEY domain, LPWSTR path, LPWSTR key, const DWORD value)
{
	HKEY hk = NULL;
	DWORD dwDisp = 0;    
    
	if (RegCreateKeyEx(domain, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hk, &dwDisp) == ERROR_SUCCESS) {
		if (RegSetValueEx(hk, key, 0, REG_DWORD, (const BYTE *) &value, sizeof value)) {
            /* SNIF... */
		}
		if (RegCloseKey(hk)) {
			return FALSE;
		}
	} else {
		return registry_write_dword(domain, path, key, value);
	}
	return TRUE;
}

static BOOL registry_write_string(HKEY domain, LPWSTR path,
                                  LPWSTR key, const LPWSTR value)
{
	HKEY hk = NULL;
    
	if (RegOpenKeyEx(domain, path, 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
		if (RegSetValueEx(hk, key, 0, REG_SZ,
                          (const BYTE *) value,
                          (1 + lstrlen(value)) * sizeof(WCHAR))) {
            /* SNIF... */
		}
		if (RegCloseKey(hk)) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL expand_storage(ULONG extra_size)
{
	DWORD store_pages;
	DWORD ram_pages;
	DWORD page_size;
    
	if (GetSystemMemoryDivision(&store_pages, &ram_pages, &page_size) == FALSE) {
		return FALSE;
	}
	store_pages += (extra_size / page_size) + 2;
	if (SetSystemMemoryDivision(store_pages) != SYSMEM_CHANGED) {
		return FALSE;
	}
	return TRUE;
}

static BOOL squeeze_storage(const DWORD security_size)
{
	DWORD store_pages;
	DWORD ram_pages;
	DWORD page_size;
    
	ForcePageout();
	if (GetSystemMemoryDivision(&store_pages, &ram_pages, &page_size) == FALSE) {
		return FALSE;
	}
	while (store_pages > 0 &&
           SetSystemMemoryDivision(store_pages) == SYSMEM_CHANGED) {
		store_pages--;
	}
	store_pages += security_size / page_size + 1U;
	if (SetSystemMemoryDivision(store_pages) != SYSMEM_CHANGED) {
		return FALSE;
	}
	return TRUE;
}

static LPWSTR get_desktop_dir(void)
{
	static LPWSTR base_path = NULL;    
	LPWSTR tested_path = TEXT("\\Windows\\Bureau");
    
	if (base_path != NULL) {
		return base_path;
	}
	if (GetFileAttributes(tested_path) != INVALID_FILE_ATTRIBUTES) {
		base_path = tested_path;
		return base_path;
	}
	base_path = tested_path = TEXT("\\Windows\\Desktop");
    
	return base_path;
}

static BOOL remove_links(void)
{
	HANDLE fh;
	WIN32_FIND_DATA found;
	LPWSTR file_name;
	LPWSTR base_path = get_desktop_dir();
	WCHAR full_path[MAX_PATH];
    
	_snwprintf(full_path, sizeof full_path, L"%s\\*.*", base_path);
	fh = FindFirstFile(full_path, &found);
	if (fh == INVALID_HANDLE_VALUE) {
		FindClose(fh);	
		return FALSE;
	}
	do {
		file_name = found.cFileName;
		_snwprintf(full_path, sizeof full_path, L"%s\\%s",
			base_path, file_name);
		DeleteFile(full_path);
	} while (FindNextFile(fh, &found) == TRUE);
	FindClose(fh);

	return TRUE;
}

static BOOL copy_files(const LPWSTR base_path, const LPWSTR mask)
{
	HANDLE fh;
	WIN32_FIND_DATA found;
	LPWSTR file_name;
	LPWSTR base_target_path = TEXT("\\Windows");
	WCHAR full_path[MAX_PATH];
	WCHAR src_path[MAX_PATH];    
	WCHAR target_path[MAX_PATH];
	ULONG size;

	_snwprintf(full_path, sizeof full_path, L"%s\\%s", base_path, mask);
	fh = FindFirstFile(full_path, &found);
	if (fh == INVALID_HANDLE_VALUE) {
		FindClose(fh);
		return FALSE;
	}
	// http://msdn.microsoft.com/en-us/library/aa454885.aspx

	do {
		size = found.nFileSizeHigh * (MAXDWORD + 1) + found.nFileSizeLow;
		file_name = found.cFileName;
		_snwprintf(src_path, sizeof src_path, L"%s\\%s",
			base_path, file_name);
		_snwprintf(target_path, sizeof target_path, L"%s\\%s",
			base_target_path, file_name);
		if (GetFileAttributes(target_path) == INVALID_FILE_ATTRIBUTES) {
			expand_storage(size);
		}
		CopyFile(src_path, target_path, TRUE);
	} while (FindNextFile(fh, &found) == TRUE);
	FindClose(fh);

	return TRUE;        
}

static BOOL create_links(YMLParserContext * const yml_parser_context)
{
	ShortCut *shortcut = yml_parser_context->shortcuts_first;
	WCHAR link[MAX_PATH];

	while (shortcut != NULL) {
		_snwprintf(link, sizeof link, L"%s\\%s.lnk", get_desktop_dir(), shortcut->link);
		create_link(shortcut->target, link, shortcut->link);
		_snwprintf(link, sizeof link, L"\\Windows\\Programs\\%s.lnk", shortcut->link);
		create_link(shortcut->target, link, shortcut->link);
		shortcut = shortcut->next;
	}
	return TRUE;
}

static BOOL spawn_autostarts(YMLParserContext * const yml_parser_context)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	AutoStart *autostart = yml_parser_context->autostarts_first;
	BOOL ret = TRUE;
	BOOL wait;
	WCHAR *pnt;
	
	ZeroMemory(&si, sizeof si);
	si.cb = sizeof si;
	ZeroMemory(&pi, sizeof pi);
	while (autostart != NULL) {
		pnt = autostart->file + wcslen(autostart->file) - 1U;
		if (pnt == autostart->file) {
			continue;
		}
		wait = TRUE;
		if (*pnt == L'&') {
			while (iswspace(*--pnt)) {
				*pnt = 0;
			}
			wait = FALSE;
		}
		if (CreateProcess(autostart->file, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == FALSE) {
			ret = FALSE;
		}
		if (wait != FALSE) {
			WaitForSingleObject(pi.hProcess, AUTOSTART_TIMEOUT);
		}
		autostart = autostart->next;
	}
	return ret;
}

static BOOL spawn_mortscripts(YMLParserContext * const yml_parser_context)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	MortScript *mortscript = yml_parser_context->mortscripts_first;
	BOOL ret = TRUE;
	static WCHAR *launcher = L"\\Evadeo\\Extra\\Sys\\Bin\\MortScript\\MortScript.exe";
	WCHAR carg[4096];
	WCHAR *pnt;
	WCHAR *cpnt;
	
	ZeroMemory(&si, sizeof si);
	si.cb = sizeof si;
	ZeroMemory(&pi, sizeof pi);

	while (mortscript != NULL) {
		pnt = mortscript->file;
		cpnt = carg;
		*cpnt++ = L'"';
		for (;;) {
			if (*pnt == 0 || cpnt == carg + (sizeof carg) - 2) {
				break;
			}
			*cpnt++ = *pnt++;
		}
		*cpnt++ = L'"';
		*cpnt = 0;
		if (CreateProcess(launcher, _wcsdup(carg), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == FALSE) {
			ret = FALSE;
		} else {
			WaitForSingleObject(pi.hProcess, MORTSCRIPT_TIMEOUT);
		}
		mortscript = mortscript->next;
	}
	return ret;
}

static BOOL set_backlight(int wanted_level, const BOOL fade)
{
	static int level = -1;

	if (fade == FALSE || level == -1) {
		level = wanted_level;
	} else if (wanted_level < level) {
		level -= 20;
		if (level < wanted_level) {
			level = wanted_level;
		}
	} else if (wanted_level > level) {
		level += 20;
		if (level > wanted_level) {
			level = wanted_level;
		}
	} else {
		return TRUE;
	}
	registry_write_dword(HKEY_CURRENT_USER,
		L"\\ControlPanel\\BackLight",
		L"BATLevel", level);
	registry_write_dword(HKEY_CURRENT_USER,
		L"\\ControlPanel\\BackLight",
		L"ACLevel", level);
	registry_write_dword(HKEY_CURRENT_USER,
		L"\\ControlPanel\\BackLight",
		L"BATBacklightOnTap", 0);
	registry_write_dword(HKEY_CURRENT_USER,
		L"\\ControlPanel\\BackLight",
		L"ACBacklightOnTap", 0);

	return TRUE;
}

static BOOL update_registry(YMLParserContext * const yml_parser_context)
{
	if (yml_parser_context->is_explorer == FALSE) {
		return FALSE;
	}
	if (yml_parser_context->is_explorer != FALSE &&
		yml_parser_context->disable_usbfn != FALSE) {
		registry_delete_value(HKEY_LOCAL_MACHINE,
			L"\\Drivers\\BuiltIn\\SC2450USBFN",
			L"Dll");
	}
	registry_delete_value(HKEY_LOCAL_MACHINE,
		L"\\Explorer\\Desktop",
		L"{000214A0-0000-0000-C000-000000000046}");		
	registry_delete_value(HKEY_LOCAL_MACHINE,
		L"\\Explorer\\Desktop",
		L"{000214A1-0000-0000-C000-000000000046}");
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\Explorer",
		L"UseCompatibleBGImage", 1);
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\Explorer",
		L"ExpandControlPanel", 0);
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\Software\\Microsoft\\Shell",
		L"Evadeo", 1);
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\Software\\Microsoft\\Shell\\Autohide",
		L"Default", 0);
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\Software\\Microsoft\\Shell\\OnTop",
		L"Default", 1);
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\Explorer",
		L"ShowExt", 1);
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\Explorer",
		L"ViewAll", 1);
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\Explorer",
		L"ShowSys", 0);
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\Explorer",
		L"UseRecycleBin", 0);

	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\GWE\\Menu",
		L"AniType", 0);    
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\GWE",
		L"Animate", 0);
		
	registry_write_string(HKEY_LOCAL_MACHINE,
		L"\\System\\Explorer\\Shell Folders",
		L"My Documents", L"\\Evadeo\\Extra\\Sys\\Documents");
	registry_write_string(HKEY_LOCAL_MACHINE,
		L"\\System\\Explorer\\Shell Folders",
		L"Application Data", L"\\Evadeo\\Extra\\Sys\\Application Data");
	registry_write_string(HKEY_LOCAL_MACHINE,
		L"\\System\\Explorer\\Shell Folders",
		L"Program Files", L"\\Evadeo\\Extra\\Sys\\Program Files");

	if (yml_parser_context->background != NULL)	{
		const WCHAR *wallpaper = yml_parser_context->background;

		if (GetFileAttributes(wallpaper) != INVALID_FILE_ATTRIBUTES) {
# if 0
			registry_create_dword(HKEY_CURRENT_USER,
				L"\\ControlPanel\\Desktop",
				L"WallpaperStyle", 1);
# endif
			registry_write_string(HKEY_CURRENT_USER,
				L"\\ControlPanel\\Desktop",
				L"Wallpaper", (WCHAR *) wallpaper);
			SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (WCHAR *) wallpaper,
				SPIF_SENDWININICHANGE);
			PostMessage(GetDesktopWindow(), WM_SETTINGCHANGE, 0, 0);
		}
	}
	return TRUE;
}

static BOOL refresh_desktop(void)
{
	INPUT input;

	InvalidateRect(NULL, NULL, TRUE);
	InvalidateRect(GetDesktopWindow(), NULL, TRUE);
	UpdateWindow(GetDesktopWindow());

	ZeroMemory(&input, sizeof input);
	input.type = INPUT_KEYBOARD;
	input.ki.wVk = VK_F5;
	input.ki.dwFlags = 0;
	SendInput(1U, &input, sizeof input);

	return TRUE;
}

static BOOL auto_backlight(YMLParserContext * const yml_parser_context)
{
	time_t now;
	struct tm *ttm;
	int tm_hour;
	int level;

	if (yml_parser_context->backlight > 100 || yml_parser_context->backlight < -1) {
		return TRUE;
	}
	if (yml_parser_context->backlight >= 0 && yml_parser_context->backlight <= 100) {
		level = yml_parser_context->backlight * 255 / 100;
		set_backlight(level, FALSE);
		return TRUE;
	}
	time(&now);
	if (now < (time_t) 1232702025 || (ttm = localtime(&now)) == NULL) {
		set_backlight(128, FALSE);
		return FALSE;
	}
	ttm = localtime(&now);
	tm_hour = ttm->tm_hour;
	if ((tm_hour > 17 && tm_hour < 20) || (tm_hour > 5 && tm_hour < 8)) {
		level = 96;
	} else if (tm_hour >= 20 || tm_hour <= 6) {
		level = 32;
	} else if (tm_hour > 11 && tm_hour < 16) {
		level = 255;
	} else {
		level = 200;
	}
	set_backlight(level, TRUE);

	return TRUE;
}

static BOOL set_sound_volume(DWORD dwVolume)
{
	UINT id;
	WAVEFORMATEX wf;
	HWAVEOUT hwo;
	
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = 2;
	wf.nSamplesPerSec = 8000 * 1000;
	wf.wBitsPerSample = 8;
	wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
	wf.cbSize = 0;
	dwVolume |= (dwVolume << 16);
	waveOutSetVolume(0, dwVolume);
	for (id = 0; id < waveOutGetNumDevs(); id++) {	
		if (waveOutOpen(&hwo, id, &wf, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
			waveOutSetVolume(hwo, dwVolume);
			waveOutClose(hwo);
		}	
	}	
#if 0	
	registry_create_dword(HKEY_CURRENT_USER, L"\\ControlPanel\\Volume", L"Volume", dwVolume);
	registry_create_dword(HKEY_CURRENT_USER, L"\\ControlPanel\\Volume", L"PlaySound", dwVolume);
#endif	
	
	return TRUE;
}

static int yml_parser_get_str_pair(YMLParserStrPair *str_pair, WCHAR *str)
{
	WCHAR *str1;
	WCHAR *str2;
	WCHAR *pnt;
	
	str_pair->str1 = str_pair->str2 = NULL;
	str1 = skip_spaces(str);
	if (*str1 == 0) {
		return -1;
	}
	if ((pnt = wcschr(str1, L':')) == NULL) {
		return -1;
	}
	str2 = skip_spaces(pnt + 1);
	if (*str2 == 0) {
		return -1;
	}
	*pnt-- = 0;
	while (iswspace(*pnt)) {
		*pnt-- = 0;
	}
	pnt = str2 + wcslen(str2) - 1U;
	while (iswspace(*pnt)) {
		*pnt-- = 0;
	}
	if (*str1 == 0 || *str2 == 0) {
		return -1;
	}
	str_pair->str1 = str1;
	str_pair->str2 = str2;
	
	return 0;
}

WCHAR *yml_parser_get_str(WCHAR *line)
{
	WCHAR *pnt;
	WCHAR *str;
	
	str = skip_spaces(line);
	if (*str == 0) {
		return NULL;
	}
	pnt = str + wcslen(str) - 1U;
	while (iswspace(*pnt)) {
		*pnt-- = 0;
	}
	if (*pnt == 0) {
		return NULL;
	}
	return str;
}

int yml_parser_get_bool(const WCHAR *str)
{
	if (wcscmp(str, L"1") == 0 || _wcsicmp(str, L"true") == 0 ||
		_wcsicmp(str, L"oui") == 0 || _wcsicmp(str, L"yes") == 0) {
		return 1;
	}
	if (wcscmp(str, L"0") == 0 || _wcsicmp(str, L"false") == 0 ||
		_wcsicmp(str, L"non") == 0 || _wcsicmp(str, L"no") == 0) {
		return 0;
	}
	return -1;
}

static int yml_parser_section_raccourcis(YMLParserContext *yml_parser_context, WCHAR *line)
{
	YMLParserStrPair str_pair;
	ShortCut *shortcut;
	
	if (yml_parser_get_str_pair(&str_pair, line) != 0) {
		return -1;
	}
	if ((shortcut = malloc(sizeof *shortcut)) == NULL) {
		return -1;
	}
	shortcut->link = _wcsdup(str_pair.str1);
	shortcut->target = _wcsdup(str_pair.str2);
	if (shortcut->link == NULL || shortcut->target == NULL) {
		free(shortcut->link);
		free(shortcut);
		return -1;
	}
	shortcut->next = NULL;
	if (yml_parser_context->shortcuts_last != NULL) {
		yml_parser_context->shortcuts_last->next = shortcut;
	} else {
		yml_parser_context->shortcuts_first = shortcut;
	}
	yml_parser_context->shortcuts_last = shortcut;	
	
	return 0;
}

static int yml_parser_section_lancement(YMLParserContext *yml_parser_context, WCHAR *line)
{
	YMLParserStrPair str_pair;
	WCHAR *str;
	AutoStart *autostart;
	
	if ((str = yml_parser_get_str(line)) == NULL) {
		return -1;
	}
	if ((autostart = malloc(sizeof *autostart)) == NULL) {
		return -1;
	}
	if ((autostart->file = _wcsdup(str)) == NULL) {
		free(autostart);
		return -1;
	}
	autostart->next = NULL;
	if (yml_parser_context->autostarts_last != NULL) {
		yml_parser_context->autostarts_last->next = autostart;
	} else {
		yml_parser_context->autostarts_first = autostart;		
	}
	yml_parser_context->autostarts_last = autostart;
	
	return 0;
}

static int yml_parser_section_mortscript(YMLParserContext *yml_parser_context, WCHAR *line)
{
	YMLParserStrPair str_pair;
 	WCHAR*str;
	MortScript *mortscript;
	
	if ((str = yml_parser_get_str(line)) == NULL) {
		return -1;
	}
	if ((mortscript = malloc(sizeof *mortscript)) == NULL) {
		return -1;
	}
	if ((mortscript->file = _wcsdup(str)) == NULL) {
		free(mortscript);
		return -1;
	}
	mortscript->next = NULL;
	if (yml_parser_context->mortscripts_last != NULL) {
		yml_parser_context->mortscripts_last->next = mortscript;
	} else {
		yml_parser_context->mortscripts_first = mortscript;		
	}
	yml_parser_context->mortscripts_last = mortscript;
	
	return 0;
}

static void yml_parser_free_context(YMLParserContext *yml_parser_context)
{
	ShortCut *shortcut;
	ShortCut *shortcut_next;
	AutoStart *autostart;
	AutoStart *autostart_next;
	
	shortcut = yml_parser_context->shortcuts_first;
	while (shortcut != NULL) {
		shortcut_next = shortcut->next;
		free(shortcut->link); shortcut->link = NULL;
		free(shortcut->target); shortcut->target = NULL;
		free(shortcut); shortcut->next = NULL;
		shortcut = shortcut_next;
	}

	autostart = yml_parser_context->autostarts_first;
	while (autostart != NULL) {
		autostart_next = autostart->next;
		free(autostart->file); autostart->file = NULL;
		free(autostart); autostart->next = NULL;
		autostart = autostart_next;
	}
	
	free(yml_parser_context->background); yml_parser_context->background = NULL;
}

static int yml_parser_section_reglages(YMLParserContext *yml_parser_context, WCHAR *line)
{
	YMLParserStrPair str_pair;
	const WCHAR *keyword, *val;

	if (yml_parser_get_str_pair(&str_pair, line) != 0) {
		return -1;
	}
	keyword = str_pair.str1;
	val = str_pair.str2;
	if (_wcsicmp(keyword, L"Papier peint") == 0) {
		yml_parser_context->background = _wcsdup(val);
		return 0;
	}
	if (_wcsicmp(keyword, L"Luminosite") == 0) {
		yml_parser_context->backlight = wcstol(val, NULL, 10);
		return 0;
	}
	if (_wcsicmp(keyword, L"Volume") == 0) {
		yml_parser_context->volume = wcstol(val, NULL, 10);
		return 0;
	}
	if (_wcsicmp(keyword, L"Effacement fichiers temporaires") == 0) {
		const int v = yml_parser_get_bool(val);

		if (v == 0) {
			yml_parser_context->delete_temporary_files = FALSE;
		}
		return 0;
	}
	if (_wcsicmp(keyword, L"Fonction USB a chaud") == 0) {
		const int v = yml_parser_get_bool(val);

		if (v == 1) {
			yml_parser_context->disable_usbfn = FALSE;
		}
		return 0;
	}
	if (_wcsicmp(keyword, L"Evadeo Boost") == 0) {
		const int v = yml_parser_get_bool(val);

		if (v == 1) {
			yml_parser_context->enable_boost = TRUE;
		}
		return 0;
	}
	return -1;
}

YMLParserSection yml_parser_sections[] = {
	{ L"Raccourcis", yml_parser_section_raccourcis },
	{ L"Lancement", yml_parser_section_lancement },
	{ L"Mortscript", yml_parser_section_mortscript },
	{ L"Reglages", yml_parser_section_reglages },
	{ NULL, NULL }
};

static int yml_parser_change_section(YMLParserContext *yml_parser_context, WCHAR *line)
{
	WCHAR *pnt;
	YMLParserSection *scanned_yml_parser_section = yml_parser_sections;
	
	if ((pnt = wcsrchr(line, L':')) == NULL) {
		return -1;
	}
	*pnt-- = 0;
	while (pnt != line) {
		if (!iswspace(*pnt)) {
			break;
		}
		*pnt-- = 0;
	}
	while (scanned_yml_parser_section->section_name != NULL) {
		if (_wcsicmp(line, scanned_yml_parser_section->section_name) == 0) {
			yml_parser_context->current_section = scanned_yml_parser_section;
			return 0;
		}
		scanned_yml_parser_section++;
	}
	return -1;
}

static int yml_parser(YMLParserContext *yml_parser_context, const WCHAR *file)
{	
	char line[4096];
	WCHAR wline[4096];
	FILE *fp;
	WCHAR *pnt;
	
	yml_parser_context->current_section = NULL;	
	if ((fp = _wfopen(file, L"rt")) == NULL) {
		return -1;
	}
	while (fgets(line, sizeof line, fp) != NULL) {
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, line, -1, wline, sizeof wline);
		pnt = skip_spaces(wline);
		if (*pnt == L'#') {
			continue;
		}
		chomp(pnt);
		if (*pnt == 0) {
			continue;
		}
		if (!isspace(*wline)) {
			if (yml_parser_change_section(yml_parser_context, wline) != 0) {
				yml_parser_context->current_section = NULL;
			}
			continue;			
		}
		if (yml_parser_context->current_section == NULL ||
			yml_parser_context->current_section->handler == NULL) {
			continue;
		}
		yml_parser_context->current_section->handler(yml_parser_context, wline);
	}
	fclose(fp);

	return 0;
}

static const WCHAR *get_startup_app(YMLParserContext * const yml_parser_context)
{
	static const WCHAR *startup = L"\\Windows\\Explorer.exe";
	static const WCHAR *alt_startup = L"\\Evadeo\\Extra\\Applications\\Utilitaires\\TC.exe";
	
	if (GetFileAttributes(L"\\Windows\\Desktop") == INVALID_FILE_ATTRIBUTES) {
		yml_parser_context->is_explorer = FALSE;
		return alt_startup;
	}
	yml_parser_context->is_explorer = TRUE;
	
	return startup;
}

static BOOL delete_temporary_files(YMLParserContext * const yml_parser_context)
{
	SHFILEOPSTRUCT fop;
	static const WCHAR *dirs[] = {
		L"\\Evadeo\\.Spotlight-V100\0",
		L"\\Evadeo\\.Trashes\0",
		L"\\Evadeo\\Recycled\0",
		L"\\Evadeo\\.DS_Store\0",
		L"\\Evadeo\\._*.*\0",
		L"\\Evadeo\\*.tmp\0",
		L"\\Evadeo\\.fseventsd\0",
		L"\\Evadeo\\.TemporaryItems\0",
		L"\\Evadeo\\evadeo\\compegps_log.txt\0",
		L"\\Evadeo\\evadeo\\temp\\*.*\0",
		NULL
	};
	const WCHAR **dir = dirs;
	FILE *fp;
	
	ZeroMemory(&fop, sizeof fop);
	fop.hwnd = GetDesktopWindow();
	fop.wFunc = FO_DELETE;
	fop.pTo = NULL;
	fop.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI;
	while (*dir != NULL) {
		fop.pFrom = *dir++;
		SHFileOperation(&fop);
	}
	if ((fp = _wfopen(L"\\Evadeo\\.metadata_never_index", L"wb")) != NULL) {
		fclose(fp);
	}
	return TRUE;
}

static LRESULT wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
	static BOOL last = FALSE;
	static int close_delay = FRAME_RATE / 2;
		
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
			
			if ((fp = _wfopen(L"\\Evadeo\\Extra\\Sys\\Media\\ACCUEIL.BMP", L"rb")) == NULL) {
				return -1;
			}
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
#if 0			
			a = height;
			do {
				a--;
				*tp++ = - (width - 1) * 3;
			} while (a > 0);
#endif			
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
			} else {
				if (--close_delay == 0) {
					DestroyWindow(hWnd);
				}
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
	return DefWindowProc(hWnd, msg, wParam, lParam); 
}

static BOOL fix_gps_ini(void)
{
	char line[8192];
	static const WCHAR *filename_in = L"\\Evadeo\\evadeo\\gps.ini";
	static const WCHAR *filename_out = L"\\Evadeo\\evadeo\\gps-fixed.ini";
	static const WCHAR *filename_old = L"\\Evadeo\\evadeo\\gps-old.ini";		
	FILE *fp;
	FILE *fpo;
	
	if (GetFileAttributes(L"\\Evadeo\\x.eva") == INVALID_FILE_ATTRIBUTES ||
		 GetFileAttributes(L"\\Windows\\Desktop") == INVALID_FILE_ATTRIBUTES) {
		return TRUE;	
	}
	if ((fp = _wfopen(filename_in, L"rt")) == NULL) {
		return FALSE;
	}
	if ((fpo = _wfopen(filename_out, L"wt")) == NULL) {
		fclose(fp);
		return FALSE;
	}
	while (fgets(line, sizeof line, fp) != NULL) {
		if (strncmp(line, "config0=", sizeof "config0=" - 1) == 0) {
			strcpy(line, "config0=1,0,0,57600,1,-2,0,0.0,NMEA,0,38400\n");
		}
		fputs(line, fpo);
	}
	fclose(fp);
	if (fclose(fpo) != 0) {
		return FALSE;
	}
	DeleteFile(filename_old);
	if (MoveFile(filename_in, filename_old)) {
		if (MoveFile(filename_out, filename_in)) {
			DeleteFile(filename_old);
			return TRUE;
		}
		MoveFile(filename_old, filename_in);		
	}
	DeleteFile(filename_out);
	
	return FALSE;
}

static BOOL intro(HINSTANCE hInstance)
{
	WNDCLASS wnd_cls;
	HWND h_wnd;
	MSG msg;
	
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
		return TRUE;
	}
	h_wnd = CreateWindow(L"Evadeo Splash", L"Evadeo Splash",
		WS_VISIBLE, 0, 0,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL);
	if (h_wnd == NULL) {
		return FALSE;
	}
	ShowWindow(h_wnd, SW_SHOW);
	UpdateWindow(h_wnd);
	while (GetMessage(&msg, NULL, 0, 0)) {
      	TranslateMessage(&msg);
      	DispatchMessage(&msg);
   	}
	DestroyWindow(h_wnd);
	UnregisterClass(wnd_cls.lpszClassName, NULL);
	
	return TRUE;
}

static LRESULT wnd_control_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	CONST WCHAR *startup;
	WNDCLASS wnd_cls;
	
	if (msg == ERROR_GENERIC_NOT_MAPPED) { // 1360
#if 0		
		ZeroMemory(&si, sizeof si);
		si.cb = sizeof si;
		ZeroMemory(&pi, sizeof pi);
		
		CreateProcess(L"\\Evadeo\\Extra\\Applications\\Utilitaires\\tc.exe", NULL,
			NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, 1000 * 1000);
#else
		if (!FindWindow(L"COMPEPOCKETMAINW", L"TwoNav")) {
			FileSystemPowerFunction(FSNOTIFY_POWER_OFF);
			PowerOffSystem();
			FileSystemPowerFunction(FSNOTIFY_POWER_ON);
		}
#endif		
		return 1;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,	int nCmdShow)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	CONST WCHAR *startup;
	YMLParserContext yml_parser_context;
	int poc = 60;
	WNDCLASS wnd_cls;
	HWND h_wnd;
	MSG msg;

	intro(hInstance);

	ZeroMemory(&si, sizeof si);
	si.cb = sizeof si;
	ZeroMemory(&pi, sizeof pi);

	yml_parser_context.shortcuts_first = yml_parser_context.shortcuts_last = NULL;
	yml_parser_context.autostarts_first = yml_parser_context.autostarts_last = NULL;
	yml_parser_context.mortscripts_first = yml_parser_context.mortscripts_last = NULL;
	yml_parser_context.background = _wcsdup(L"\\Evadeo\\Extra\\Sys\\Media\\fond.bmp");
	yml_parser_context.backlight = -1;
	yml_parser_context.volume = -1;
	yml_parser_context.delete_temporary_files = TRUE;
	yml_parser_context.disable_usbfn = TRUE;
	yml_parser_context.enable_boost = FALSE;	

	startup = get_startup_app(&yml_parser_context);
	yml_parser(&yml_parser_context, L"\\Evadeo\\Extra\\Sys\\debridage.ini");
	update_registry(&yml_parser_context);
	if (yml_parser_context.enable_boost != FALSE) {
		CreateProcess(L"\\Evadeo\\Extra\\Applications\\Utilitaires\\evadeo-boost.exe", L"/QUIET",
			NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, 1000 * 10);
	}
	auto_backlight(&yml_parser_context);
	remove_links();
	if (yml_parser_context.volume >= 0 && yml_parser_context.volume <= 100) {
		set_sound_volume((yml_parser_context.volume * 65535) / 100);
	}
	if (!CreateProcess(startup, NULL, // /e
		NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		WCHAR io[1000];

		_snwprintf(io, sizeof io, L"Erreur [%d] pour charger [%s]\n", (int) GetLastError(), startup);
		MessageBox(NULL, io, TEXT("Débridage Evadeo"), MB_OK);
	}
	if (yml_parser_context.is_explorer != FALSE) {
		while (FindWindow(L"HHTaskBar", L"") == NULL) {
			Sleep(500);
		}
		remove_links();	
		create_links(&yml_parser_context);
	}
	if (yml_parser_context.delete_temporary_files != FALSE) {
		delete_temporary_files(&yml_parser_context);
	}
	if (yml_parser_context.is_explorer != FALSE) {
		copy_files(L"\\Evadeo\\Extra\\Sys\\DLLs", L"*.DLL");
		copy_files(L"\\Evadeo\\Extra\\Sys\\DLLs", L"*.INI");
	}
	copy_files(L"\\Evadeo\\Extra\\Sys\\DLLs2", L"*.DLL");
	copy_files(L"\\Evadeo\\Extra\\Sys\\DLLs2", L"*.INI");
	update_registry(&yml_parser_context);
	fix_gps_ini();
#if 0	
	refresh_desktop();
#endif	

	WaitForSingleObject(pi.hProcess, 2000 /* INFINITE */);
	
	spawn_autostarts(&yml_parser_context);
	spawn_mortscripts(&yml_parser_context);	
	
	ZeroMemory(&wnd_cls, sizeof wnd_cls);
	wnd_cls.style = 0; // CS_HREDRAW | CS_VREDRAW;
	wnd_cls.hInstance = hInstance;
	wnd_cls.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd_cls.hbrBackground = NULL;
	wnd_cls.lpszClassName = L"Evadeo Splash";
	wnd_cls.lpfnWndProc = (WNDPROC) wnd_control_proc;
	RegisterClass(&wnd_cls);
	
	h_wnd = FindWindow(L"Evadeo Splash", L"Evadeo Splash");
	if (h_wnd) {
		return 1;
	}
	h_wnd = CreateWindow(L"Evadeo Splash", L"Evadeo Splash",
			0, 0, 0,
			CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, NULL, hInstance, NULL);
	if (h_wnd == NULL) {
		return 1;
	}
	ShowWindow(h_wnd, SW_HIDE);
	UpdateWindow(h_wnd);
	for(;;) {
		SetTimer(h_wnd, 1, 2 * 1000, NULL);
		while (GetMessage(&msg, NULL, 0, 0) && msg.message != WM_TIMER) {
	      	TranslateMessage(&msg);
	      	DispatchMessage(&msg);			
		}
		if (yml_parser_context.is_explorer != FALSE &&
			FindWindow(L"DesktopExplorerWindow", L"") == NULL) {
				TerminateProcess(pi.hProcess, 1);
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				Sleep(1000);
				CreateProcess(startup, NULL, // /e
					NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		}
		auto_backlight(&yml_parser_context);
		if (poc == 0) {
			ForcePageout();
			poc = 60;
		} else {
			poc = 0;
		}
	}
	DestroyWindow(h_wnd);
	UnregisterClass(wnd_cls.lpszClassName, NULL);
	yml_parser_free_context(&yml_parser_context);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return 0;
}
