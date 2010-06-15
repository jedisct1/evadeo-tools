#define COBJMACROS 1

#include <windows.h>
#include <winable.h>
#include <ntdef.h>
#include <winreg.h>
#include <shellapi.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

extern VOID ForcePageout(void);

#ifndef SYSMEM_CHANGED
# define SYSMEM_CHANGED   0
# define SYSMEM_MUSTREBOOT 1
# define SYSMEM_REBOOTPENDING 2
# define SYSMEM_FAILED 3
#endif

#ifndef POWER_STATE_RESET 
# define POWER_STATE_RESET (DWORD) 0x00800000
#endif

DWORD SetSystemPowerState(LPCWSTR psState, DWORD StateFlags, DWORD Options);

BOOL registry_key_exists(HKEY domain, LPWSTR path, LPWSTR key)
{
	BOOL ret = FALSE;
	HKEY hk = NULL;   

	if (RegOpenKeyEx(domain, path, 0,
	KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
		if (RegQueryValueEx(hk, key, 0, NULL, NULL, NULL)
		== ERROR_SUCCESS) {
			ret = TRUE;
		}
		if (RegCloseKey(hk)) {
			return FALSE;
		}
	}
	return ret;    
}

BOOL registry_write_dword(HKEY domain, LPWSTR path,
	LPWSTR key, const DWORD value)
{
	HKEY hk = NULL;

	if (RegOpenKeyEx(domain, path, 0,
	KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
		if (RegSetValueEx(hk, key, 0, REG_DWORD,
		(const BYTE *) &value, sizeof value)) {
		/* SNIF... */
		}
		if (RegCloseKey(hk)) {
			return FALSE;
		}
	}
	return TRUE;
}

BOOL registry_create_dword(HKEY domain, LPWSTR path,
	LPWSTR key, const DWORD value)
{
	HKEY hk = NULL;
	DWORD dwDisp = 0;    

	if (RegCreateKeyEx(domain, path, 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
	NULL, &hk, &dwDisp) == ERROR_SUCCESS) {
		if (RegSetValueEx(hk, key, 0, REG_DWORD,
		(const BYTE *) &value, sizeof value)) {
		/* SNIF... */
		}
		if (RegCloseKey(hk)) {
			return FALSE;
		}
	}
	return TRUE;
}

BOOL registry_write_string(HKEY domain, LPWSTR path,
	LPWSTR key, const char *value)
{
	HKEY hk = NULL;

	if (RegOpenKeyEx(domain, path, 0,
	KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
		if (RegSetValueEx(hk, key, 0, REG_SZ,
		(const BYTE *) value, strlen(value) + 1)) {
		/* SNIF... */
		}
		if (RegCloseKey(hk)) {
			return FALSE;
		}
	}
	return TRUE;
}

BOOL update_registry(void) {
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\StorageManager\\Profiles\\"
		"SDMemory\\FATFS",
		L"DataCacheSize", 0);
#if 1
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\StorageManager\\Profiles\\"
		"SDMemory\\FATFS",
		L"CacheSize", 4096);
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\StorageManager\\Profiles\\"
		"SDMemory\\FATFS",
		L"EnableCache", 1);
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\StorageManager\\Profiles\\"
		"SDMemory\\FATFS",
		L"EnableCacheWarm", 1);
#endif
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\StorageManager\\Profiles\\"
		"SDMemory\\FATFS",
		L"EnableWriteBack", 1);
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\StorageManager\\Profiles\\"
		"SDMemory\\FATFS",
		L"FatCacheSize", 0);
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\StorageManager\\Profiles\\"
		"SDMemory\\FATFS",
		L"Flags", 68);   
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\Drivers\\SDCARD\\ClientDrivers\\"
		"Class\\SDMemory_Class",
		L"BlockTransferSize", 16);
#if 1
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\Drivers\\SDCARD\\ClientDrivers\\"
		"Class\\SDMemory_Class",
		L"SingleBlockWrites", 1);
#endif
#if 1
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\Drivers\\SDCARD\\ClientDrivers\\"
		"Class\\SDMemory_Class",
		L"IOCTL", 4);
#endif

#if 1
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\StorageManager\\Filters",
		L"fsreplxfilt", 4096);
#endif

#if 1
	registry_write_dword(HKEY_LOCAL_MACHINE,
		L"\\Drivers\\SDCARD\\ClientDrivers\\"
		"Custom\\MANF-02DB-CARDID-0002-FUNC-1",
		L"Dll", 0);
#endif

#if 1
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\Drivers\\BuiltIn\\SDBusDriver",
		L"PowerUpPollingInterval", 2);
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\Drivers\\BuiltIn\\SDBusDriver",
		L"PowerUpPollingTime", 2000);
#endif

#if 1
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\GDI\\GlyphCache",
		L"Limit", 32768);
#endif

#if 1
	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\CurrentControlSet\\Control\\"
		"Power\\States\\Suspend",
		L"dsk1", 2);
#endif

	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\GWE\\Menu",
		L"AniType", 0);

	registry_create_dword(HKEY_LOCAL_MACHINE,
		L"\\System\\GWE",
		L"Animate", 0);

	return TRUE;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	BOOL quiet = FALSE;
	
	if (wcscmp(lpCmdLine, L"/QUIET") == 0) {
		quiet = TRUE;
	}
	if (GetFileAttributes(L"\\Evadeo\\x.eva") == INVALID_FILE_ATTRIBUTES ||
		GetFileAttributes(L"\\Windows\\Desktop") == INVALID_FILE_ATTRIBUTES) {
		if (quiet == FALSE) {
			MessageBox(NULL, L"Cet outil n'est compatible qu'avec Evadeo X50 et X60",
				TEXT("Evadeo Boost"),
				MB_OK);
		}
		return 1;
	}
	if (registry_key_exists(HKEY_LOCAL_MACHINE,
		L"\\System\\StorageManager\\Profiles\\"
		"SDMemory\\FATFS", L"EnableCacheWarm") == TRUE) {
		if (quiet == FALSE) {
			MessageBox(NULL, L"Votre Evadeo est deja en mode turbo",
				TEXT("Evadeo Boost"),
				MB_OK);
		}
		return 0;
	}
	if (quiet == FALSE) {
		MessageBox(NULL, L"Evadeo Boost v1.2 par Axey pour www.evadeiste.fr\r\n"
			"Cliquez sur OK pour redemarrer l'Evadeo en mode turbo",
			TEXT("Evadeo Boost"), MB_OK);
	}
	update_registry();
	ForcePageout();    
	Sleep(1000);
	ForcePageout();
	SetSystemPowerState(NULL, POWER_STATE_RESET, 0);

	return 0;
}
