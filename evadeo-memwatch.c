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

BOOL mem_watch(void)
{
    const LPWSTR file_name = L"\\Evadeo\\evadeo-memwatch.log";
    const char *file_name_c = "\\Evadeo\\evadeo-memwatch.log";    
    WCHAR msg[200];
    char line[1000];
    FILE *fp;
    MEMORYSTATUS mem_status;
    time_t now;
    struct tm *ttm;
    
    _snwprintf(msg, sizeof msg,
	       L"Evadeo-memwatch, par Axey.\n\n"
	       "L'enregistrement de la memoire debute dans [%s]", file_name);
    fp = fopen(file_name_c, "w");
    if (fp == NULL) {
	_snwprintf(msg, sizeof msg,
		   L"Erreur (%d) en ecrivant [%s] - plus d'espace disque ?",
		   GetLastError(), file_name);
	MessageBox(NULL, msg, L"Evadeo-memwatch", MB_OK);	    
	return FALSE;	
    }
    fputs("Heure\tPourcentage de memoire occupee\tMemoire physique totale\tMemoire physique libre\t"
	  "Memoire virtuelle totale\tMemoire virtuelle libre\n", fp);
    fclose(fp);    
    MessageBox(NULL, msg, L"Evadeo-memwatch", MB_OK);
    for(;;) {	
	GlobalMemoryStatus(&mem_status);
	fp = fopen(file_name_c, "a");
	if (fp == NULL) {
	    _snwprintf(msg, sizeof msg,
		       L"Erreur (%d) en ouvrant [%s]",
		       GetLastError(), file_name);
	    MessageBox(NULL, msg, L"Evadeo-memwatch", MB_OK);	    
	    break;
	}
	time(&now);
	ttm = localtime(&now);
	_snprintf(line, sizeof line,
		  "%02d:%02d:%02d\t%ld %%\t%ld\t%ld\t%ld\t%ld\n",
		  (int) ttm->tm_hour, (int) ttm->tm_min, (int) ttm->tm_sec,
		  (long) mem_status.dwMemoryLoad,
		  (long) mem_status.dwTotalPhys,
		  (long) mem_status.dwAvailPhys,
		  (long) mem_status.dwTotalVirtual,
		  (long) mem_status.dwAvailVirtual);
	if (fwrite(line, (size_t) 1U, strlen(line), fp) <= 0) {
	    _snwprintf(msg, sizeof msg,
		       L"Erreur (%d) en ecrivant [%s] - plus d'espace disque ?",
		       GetLastError(), file_name);
	    MessageBox(NULL, msg, L"Evadeo-memwatch", MB_OK);	    
	    break;
	}
	fflush(fp);
	fclose(fp);	
	fp = NULL;
	Sleep(10000);
    }
    if (fp != NULL) {
	fclose(fp);
    }
    
    return TRUE;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,
	    int nCmdShow)
{
    mem_watch();
    
    return 0;
}
