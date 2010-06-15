#define COBJMACROS 1

#include <windows.h>
#include <winable.h>
#include <ntdef.h>
#include <winreg.h>
#include <shellapi.h>
#include <commctrl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <winuser.h>
#include <math.h>

#define EPHEMERIS_NUMBER 32
#define POWER_MASK 20
#define FRAME_RATE (1000 / 2)

#define GRIBOUILLI 0
#define DEBUG 0

#if DEBUG
# warning DEBUG BUILD
#endif

extern VOID ForcePageout(void);

static HWND h_test;
static UINT timer = 0;
static HANDLE global_hcom;
static BOOL taskbar_init_enabled = FALSE;

typedef double Coord;

typedef struct MinDecCoord_ {
  int deg;
  int min;
  int dec;
} MinDecCoord;

typedef struct LastPos_ {
	Coord lon;
	Coord lat;
	double hdop;
} LastPos;

static BOOL hide_taskbar(void)
{
	HWND taskBarHWnd = FindWindow(L"HHTaskBar", L"");
    
	if (taskBarHWnd == NULL) {
		return FALSE;
	}
	if (IsWindowEnabled(taskBarHWnd)) {
		taskbar_init_enabled = TRUE;
	}
//	ShowWindow(taskBarHWnd, SW_HIDE);
	EnableWindow(taskBarHWnd, FALSE);
		
	return TRUE;	
}

static BOOL restore_taskbar(void)
{
	HWND taskBarHWnd = FindWindow(L"HHTaskBar", L"");
    
	if (taskBarHWnd == NULL) {
		return FALSE;
	}
	if (taskbar_init_enabled == FALSE) {
		return TRUE;
	}
	ShowWindow(taskBarHWnd, SW_SHOW);
	EnableWindow(taskBarHWnd, TRUE);
	
	return TRUE;
}

static MinDecCoord coord_dec_to_mindec(const double a)
{
  MinDecCoord mindec;
  double min_f;

  mindec.deg = (int) a;
  min_f = (a - mindec.deg) * 60.0;
  mindec.min = (int) min_f;
  mindec.dec = ((min_f - mindec.min + 0.0005) * 1000);

  return mindec;
}

static MinDecCoord coord_nmea_to_mindec(const double a)
{
  MinDecCoord mindec;
  double min_f;

  mindec.deg = (int) a / 100;
  mindec.min = (int) a % 100;
  min_f = a - (int) a;
  mindec.dec = ((min_f + 0.0005) * 1000);

  return mindec;
}

static size_t strlcpy(char *dst, const char *src, size_t siz)
{
  char *d = dst;
  const char *s = src;
  size_t n = siz;

  if (n != 0) {
    while (--n != 0) {
      if ((*d++ = *s++) == '\0')
	break;
    }
  }  
  if (n == 0) {
    if (siz != 0) {
      *d = 0;
    }
    while (*s++);
  }
  return s - src - 1;
}

static char *tokenize(char *str, int chr_)
{
  static char *part;
  static int chr;
  char *oldpart;
  char *sep;
  
  if (str == NULL) {
    return NULL;
  }
  if (chr_ != 0) {
    chr = chr_;
    part = str;
  }
  if (*part == 0) {
    return "";
  }
  oldpart = part;
  if ((sep = strchr(part, chr)) != NULL) {
    *sep = 0;
    part = sep + 1;
  } else {
    part = "";
  }
  return oldpart;
}

static double tokenize_double(char *str)
{
  return atof(tokenize(str, 0));
}

static WCHAR *tokenize_string(char *str)
{
	char *tokenized;
	static WCHAR wstr[2000];
	
	tokenized = tokenize(str, 0);
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, tokenized, -1, wstr, sizeof wstr);

	return wstr;
}

static int tokenize_int(char *str)
{
  return (int) strtol(tokenize(str, 0), NULL, 0);
}

typedef enum FixQuality_ {
  FQ_INVALID = 0,
  FQ_GPS = 1,
  FQ_DGPS = 2,
  FQ_PPS = 3,
  FQ_RTK = 4,
  FQ_FLOAT = 5,
  FQ_ESTIMATED = 6,
  FQ_MANUAL = 7,
  FQ_SIMULATION = 8
} FixQuality;

typedef enum TridiFix_ {
  TDFX_NOFIX = 1,
  TDFX_2D = 2,
  TDFX_3D = 3
} TridiFix;

typedef struct GPStatus_ {
	BOOL initialized;

	Coord lat;
	WCHAR lat_dir[4];
	Coord lon;
	WCHAR lon_dir[4];
	FixQuality fix_quality;
	int tracked_sat_count;
	double hdop;
	double vdop;
	int altitude;
	time_t dgps_since;
	int dgps_station_id;

	TridiFix tridifix;
	int useful_sat_count;

	int time_valid_flag;
	int tow;	
	int found_ephemeris;
	
	double decibels;
	double masked_decibels;
	
	Coord avg_total_lat;
	Coord avg_total_lon;
	double avg_lonlat_counts;
	
	long int avg_total_altitude;
	double avg_altitude_counts;
} GPStatus;

static GPStatus gpstatus;

int nmea_cb_gpgga(char *sentence)
{  
	char *str;
	WCHAR *wstr;
	
	tokenize_double(sentence); /* fix_date */
	str = tokenize(sentence, 0);
	if (*str != 0) {
		gpstatus.lat = atof(str);
	}
	wstr = tokenize_string(sentence);
	if (wcslen(wstr) > 0) {
		_snwprintf(gpstatus.lat_dir, sizeof gpstatus.lat_dir, L"%s", wstr);
	}
	str = tokenize(sentence, 0);
	if (*str != 0) {
		gpstatus.lon = atof(str);
	}
	wstr = tokenize_string(sentence);
	if (wcslen(wstr) > 0) {
		_snwprintf(gpstatus.lon_dir, sizeof gpstatus.lon_dir, L"%s", wstr);
	}
	gpstatus.fix_quality = tokenize_int(sentence);
	gpstatus.tracked_sat_count = tokenize_int(sentence);
	gpstatus.hdop = tokenize_double(sentence);
	gpstatus.altitude = (int) (tokenize_double(sentence) + 0.5);
	tokenize_string(sentence); /* unité */
	tokenize_double(sentence); /* geoid separation */
	tokenize_string(sentence); /* unité */	
	gpstatus.dgps_since = tokenize_int(sentence);
	gpstatus.dgps_station_id = tokenize_int(sentence);

	return 0;
}

int nmea_cb_gpgsa(char *sentence)
{
	int sats = 0;
	int sat_count = 12;

	tokenize_string(sentence); /* auto-selection */
	gpstatus.tridifix = tokenize_int(sentence);
	do {
		if (*(tokenize(sentence, 0)) != 0) {
			sats++;
		}
	} while (--sat_count != 0);
	gpstatus.useful_sat_count = sats;
	tokenize_double(sentence); /* pdop */
	tokenize_double(sentence); /* hdop */
	gpstatus.vdop = tokenize_double(sentence);

	return 0;
}

int nmea_cb_gprmc(char *sentence)
{
	double gptime;
	int gpdate;
	SYSTEMTIME stime;
	
	gptime = tokenize_double(sentence);
	if (*(tokenize_string(sentence)) != L'V') {
		return 0;
	}
	tokenize_double(sentence); /* latitude */
	tokenize_string(sentence); /* N/S */
	tokenize_double(sentence); /* longitude */
	tokenize_string(sentence); /* E/W */
	tokenize_double(sentence); /* vitesse */
	tokenize_double(sentence); /* cap */
	gpdate = tokenize_int(sentence);
	ZeroMemory(&stime, sizeof stime);
	stime.wYear = 2000 + gpdate % 100;
	if (stime.wYear < 2009) {
		return -1;
	}
	stime.wMonth = (gpdate / 100) % 100;
	stime.wDay = (gpdate / 10000) % 100;
	stime.wHour = (int) (gptime / 10000) % 100;
	stime.wMinute = (int) (gptime / 100) % 100;
	stime.wSecond = (int) gptime % 100;
	stime.wMilliseconds = (int) ((gptime - (int) gptime) * 1000.0);
	SetSystemTime(&stime);
	
	return 0;
}

int nmea_cb_psrf151(char *sentence)
{
	unsigned int ephemeris;
	int found_ephemeris = 0;
	int bits = 32;
	
	gpstatus.time_valid_flag = tokenize_int(sentence);
	tokenize_int(sentence); /* week number */
	gpstatus.tow = tokenize_int(sentence);
	ephemeris = (unsigned int) tokenize_int(sentence);
	
	if (ephemeris == 0U && (gpstatus.tow == 0 || gpstatus.time_valid_flag == 0)) {
		ephemeris = 0xffffffff;
	}
	do {
		if ((ephemeris & 0x80000000) == 0) {
			found_ephemeris++;
		}
		ephemeris <<= 1;
	} while (--bits != 0);
	if (found_ephemeris > EPHEMERIS_NUMBER) {
		found_ephemeris = EPHEMERIS_NUMBER;
	}	
	gpstatus.found_ephemeris = found_ephemeris;
	
	return 0;
}

int nmea_cb_gpgsv(char *sentence)
{
	static double decibels = 0.0;
	static double masked_decibels = 0.0;
	double dp;
	int nb;
	int total_nb;
	int sats_per_sentence = 4;
	int snr;
	char *str;
	
	total_nb = tokenize_int(sentence); /* number of sentences */
	nb = tokenize_int(sentence);
	tokenize_int(sentence); /* number of stats in view */
	if (nb < 2) {
		decibels = 0.0;
		masked_decibels = 0.0;
	}
	do {
		tokenize_int(sentence); /* PRN */
		tokenize_int(sentence); /* elevation */
		tokenize_int(sentence); /* azimuth */
		str = tokenize(sentence, 0);
		if (*str == 0) {
			continue;
		}
		snr = atoi(str);
		if (snr > 100) {
			snr = 100;
		}
		dp = pow(10.0, (double) snr / 10.0);
		decibels += dp;
		if (snr >= POWER_MASK) {
			masked_decibels += dp;
		}
	} while (--sats_per_sentence != 0);
	if (decibels == 0.0) {
		decibels = -10000000.0;
	} else {
		decibels = 10.0 * log10(decibels);
	}
	if (masked_decibels == 0.0) {
		masked_decibels = -10000000.0;
	} else {
		masked_decibels = 10.0 * log10(masked_decibels);
	}
	if (total_nb == nb) {
		gpstatus.decibels = max(decibels, 0.0);
		gpstatus.masked_decibels = max(masked_decibels, 0.0);
	}
	return 0;
}

typedef int (*NmeaCallback)(char *str);

typedef struct NmeaSentence_ {
  const char *keyword;
  NmeaCallback callback;
} NmeaSentence;

NmeaSentence nmea_sentences[] = {
	{ "$GPGGA", nmea_cb_gpgga },
	{ "$GPGSA", nmea_cb_gpgsa },
	{ "$PSRF151", nmea_cb_psrf151 },
	{ "$GPGSV", nmea_cb_gpgsv },
	{ "$GPRMC", nmea_cb_gprmc },	
	{ NULL, (NmeaCallback) NULL }
};

static BOOL wait_any_data(HANDLE hcom)
{
	CHAR k[8192];
	DWORD readen;
	
#ifdef USHUAIA
	Sleep(100);
#else
	ReadFile(hcom, k, sizeof k - 1, &readen, NULL);
#endif	
	PurgeComm(hcom, PURGE_RXCLEAR);
	
	/* PSRF154 */
	return TRUE;
}

static BOOL send_nmea_command(HANDLE hcom, const char *cmd)
{
	char strbuf[1000];
	DWORD written;
	DWORD length;
	unsigned char cksum = 0U;
	unsigned char c;
	const char *pnt = cmd;	
	
	while ((c = (unsigned char) *pnt++) != 0) {
		cksum ^= c;
	}
	_snprintf(strbuf, sizeof strbuf, "$%s*%02X\r\n", cmd, cksum);
	length = strlen(strbuf);
	WriteFile(hcom, strbuf, length, &written, NULL);
	for (;;) {
		if (written != length) {
			Sleep(1000);
			wait_any_data(hcom);
			continue;
		}
		break;
	}
	FlushFileBuffers(hcom);
		
	return TRUE;
}

static BOOL send_sirf_command(HANDLE hcom, const unsigned char *cmd,
							  const size_t length)
{
	unsigned char strbuf[1000];
	unsigned char *pnts = strbuf;
	WORD cksum = 0U;
	size_t t = 0U;
	unsigned char c;
	DWORD written;
	DWORD size;
	
	if (length > sizeof strbuf - 8U) {
		return FALSE;
	}
	*pnts++ = 0xa0;
	*pnts++ = 0xa2;
	*pnts++ = length >> 8;
	*pnts++ = length & 0xff;
	do {
		c = *cmd++;
		cksum += (WORD) c;
		*pnts++ = c;
	} while (++t != length);
	cksum &= 0x7fff;
	*pnts++ = cksum >> 8;
	*pnts++ = cksum & 0xff;
	*pnts++ = 0xb0;
	*pnts++ = 0xb3;
	size = pnts - strbuf;
	for (;;) {
		WriteFile(hcom, strbuf, size, &written, NULL);
		if (written != size) {
			Sleep(1000);
			wait_any_data(hcom);
			continue;
		}
		break;
	}
	FlushFileBuffers(hcom);
	
	return TRUE;
}							

static HANDLE open_port(const WCHAR **ports_list, BOOL is_virtual)
{
	const WCHAR **port = ports_list;
	HANDLE hcom;
	DCB dcb;		
	
	do {
		hcom = CreateFile(*port, GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, 0, NULL);
		if (hcom == INVALID_HANDLE_VALUE) {
			continue;
		}
		if (!GetCommState(hcom, &dcb)) {
			if (is_virtual) {
				goto commstate_initialized;
			}
			CloseHandle(hcom);
			hcom = INVALID_HANDLE_VALUE;
			continue;
		}
		dcb.DCBlength = sizeof dcb;
#ifdef USHUAIA
		dcb.BaudRate = 4800;
#else		
		dcb.BaudRate = 57600;
#endif		
		dcb.fBinary = TRUE;
		dcb.fParity = TRUE;
		dcb.fOutxCtsFlow = FALSE;
		dcb.fOutxDsrFlow = FALSE;
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fTXContinueOnXoff = TRUE;
		dcb.fOutX = FALSE;
		dcb.fInX = FALSE;
		dcb.fErrorChar = FALSE;
		dcb.fNull = FALSE;
		dcb.fRtsControl = RTS_CONTROL_DISABLE;
		dcb.fAbortOnError = TRUE;
		dcb.ByteSize = 8;
		dcb.Parity = NOPARITY;
		dcb.StopBits = ONESTOPBIT;
		if (!SetCommState(hcom, &dcb)) {
			CloseHandle(hcom);
			hcom = INVALID_HANDLE_VALUE;
			continue;
		}
		commstate_initialized:
		PurgeComm(hcom, PURGE_RXCLEAR);
		PurgeComm(hcom, PURGE_TXCLEAR);		
		break;
	} while (*++port != NULL);
	
	return hcom;
}

static BOOL setup_sirf(const WCHAR **ports_list)
{ 
	WCHAR strbuf[1000];
	MSG msg;
	HANDLE hcom;
	
	hcom = open_port(ports_list, FALSE);
	if (hcom == INVALID_HANDLE_VALUE) {
		MessageBox(NULL, L"Impossible d'acceder a la puce SiRF",
		       TEXT("Evadeo Affix"), MB_OK);	
		return FALSE;
	}
#if 1
	_snwprintf(strbuf, sizeof strbuf, L"Configuration de la Puce SiRF");
	SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) strbuf);
	if (GetMessageW(&msg, NULL, 0, 0)) {
	    TranslateMessage(&msg);
	    DispatchMessageW(&msg);
	}
#endif	
	
#if 0
	{
		CHAR k[1000];
		WCHAR wk[1000];
		DWORD readen;
		
		ReadFile(hcom, k, sizeof k - 1, &readen, NULL);
		k[readen] = 0;
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, k, -1, wk, sizeof wk);
	}
#endif
	wait_any_data(hcom);
	
#if 1
	send_nmea_command(hcom, "PSRF151,01");
	send_nmea_command(hcom, "PSRF108,01");
#endif
	
#ifdef USHUAIA
	send_nmea_command(hcom, "PSRF100,0,4800,8,1,0");
	Sleep(100);
	send_nmea_command(hcom, "PSRF100,0,4800,8,1,0");
	Sleep(100);
	send_nmea_command(hcom, "PSRF100,0,4800,8,1,0");
	Sleep(100);	
#else	
	send_nmea_command(hcom, "PSRF100,0,57600,8,1,0");
	Sleep(100);
	send_nmea_command(hcom, "PSRF100,0,57600,8,1,0");
	Sleep(100);
	send_nmea_command(hcom, "PSRF100,0,57600,8,1,0");
	Sleep(100);	
#endif	
	
	{
		const unsigned char sc_nop[] = {
			0x00
		};
		send_sirf_command(hcom, sc_nop, sizeof sc_nop);
	}
	
#if 0
	{
		const unsigned char sc_production_operating_mode[] = {
			0x96,
			0x00,
			0x00, 0x06,
			0x00, 0x1E
		};
		send_sirf_command(hcom, sc_production_operating_mode, sizeof sc_production_operating_mode);
	}
#endif
	
	{
		const unsigned char sc_enable_sbas[] = {
			0x85,
			0x01,
			0x00, 0x00, 0x00, 0x00,
			0x00 };
		send_sirf_command(hcom, sc_enable_sbas, sizeof sc_enable_sbas);
	}
	
#if 0	
	{
		const unsigned char sc_set_trickle_parameters[] = {
			0x97, 0x00, 0x01, 0x03, 0xE8, 0x00, 0x00, 0x00, 0xC8
		};
		send_sirf_command(hcom, sc_set_trickle_parameters, sizeof sc_set_trickle_parameters);
	}
#endif	
	
	{
		const unsigned char sc_fast_push_to_fix[] = {
			0xA7, 0x00, 0x01, 0xD4, 0xC0, 0x00, 0x00, 0x75, 0x30, 0x00, 0x00,
			0x01, 0x2C, 0x00, 0x00		
		};
		send_sirf_command(hcom, sc_fast_push_to_fix, sizeof sc_fast_push_to_fix);				
	}
	
	{
		const unsigned char sc_set_low_power_acquisition_params[] = {
			0xA7, 0x00, 0x01, 0xD4, 0xC0, 0x00, 0x00, 0x75, 0x30, 0x00, 0x00, 0x01,
			0x2C, 0x00, 0x00
		};
		send_sirf_command(hcom, sc_set_low_power_acquisition_params, sizeof sc_set_low_power_acquisition_params);
	}
	
#if 1	
	{
		const unsigned char sc_exit_push_to_fix[] = {
			0x97, 0x00, 0x00, 0x03, 0xE8, 0x00, 0x00,0x00, 0xC8
		};
		send_sirf_command(hcom, sc_exit_push_to_fix, sizeof sc_exit_push_to_fix);
	}
#endif

	{
#if 0
		const unsigned char sc_enable_static_mode[] = {
			0x8F, 0x01
		};
		const unsigned char sc_disable_static_mode[] = {
			0x8F, 0x00
		};		
		send_sirf_command(hcom, sc_disable_static_mode, sizeof sc_disable_static_mode);
#endif		
	}

#if 1
	{
		const unsigned char sc_mode_control[] = {
			0x88, 0x00, 0x00,
			0x01, /* Degraded mode 0 - 4 (4 = disallow) */
			0x00, 0x00,
			0x00, 0x00, /* user-altitude */
			0x00, /* altitude hold mode 0 = auto */
			0x00, /* user last computed altitude */
			0x00,
			0x0a, /* degraded mode pendant 10 secondes (max 120) */
			0x03, /* dead reckoning (max 120) */
			0x01  /* track smoothing */
		};
		send_sirf_command(hcom, sc_mode_control, sizeof sc_mode_control);
	}
#endif

	{
		const unsigned char sc_mask_control[] = {
			0x89,
			0x00, /* auto PDOP/HDOP */
			0x08, /* GDOP (1-50) */
			0x08, /* PDOP */
			0x08  /* HDOP */
		};
		send_sirf_command(hcom, sc_mask_control, sizeof sc_mask_control);
	}
	
	{
		const unsigned char sc_dgps_control[] = {
			0x8a,
			0x00, /* auto (1 = exclusive) */
			0x1E  /* timeout */
		};
		send_sirf_command(hcom, sc_dgps_control, sizeof sc_dgps_control);
	}

#if 0	
	{
		const unsigned char sc_elevation_mask[] = {
			0x8b,
			0x00, 0x16, /* tracking mask */
			0x00, 0x32  /* navigation mask */
		};
		send_sirf_command(hcom, sc_elevation_mask, sizeof sc_elevation_mask);
	}
#endif
	
	{
		const unsigned char sc_power_mask[] = {
			0x8c,
			0x14, /* tracking mask (pas gere) */
			0x14
		};
		send_sirf_command(hcom, sc_power_mask, sizeof sc_power_mask);
	}

#if 1
	{		
		const unsigned char sc_back_to_nmea[] = {
			0x81, 0x02, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01,
#ifdef USHUAIA
			0x12, 0xC0 /* 4800 */
#else			
			0xE1, 0x00 /* 57600 */
#endif			
		};
		send_sirf_command(hcom, sc_back_to_nmea, sizeof sc_back_to_nmea);
		send_sirf_command(hcom, sc_back_to_nmea, sizeof sc_back_to_nmea);
		send_sirf_command(hcom, sc_back_to_nmea, sizeof sc_back_to_nmea);				
	}
#endif				
		
	send_nmea_command(hcom, "PSRF103,0,0,0,1"); // GGA
	send_nmea_command(hcom, "PSRF103,1,0,1,1"); // GLL
	send_nmea_command(hcom, "PSRF103,2,0,5,1"); // GSA
	send_nmea_command(hcom, "PSRF103,3,0,5,1"); // GSV
	send_nmea_command(hcom, "PSRF103,4,0,1,1"); // RMC
	send_nmea_command(hcom, "PSRF103,5,0,1,1"); // VTG
	send_nmea_command(hcom, "PSRF103,6,0,1,1"); // MSS
	send_nmea_command(hcom, "PSRF103,7,0,1,1"); // ?
	send_nmea_command(hcom, "PSRF103,8,0,1,1"); // ZDA
	send_nmea_command(hcom, "PSRF103,9,0,1,1"); // ?
	
	wait_any_data(hcom);	
	
	send_nmea_command(hcom, "PSRF112,140,6,1"); // message rate for ephemeris
	
	wait_any_data(hcom);
	PurgeComm(hcom, PURGE_RXCLEAR);	
	CloseHandle(hcom);
	
#if 1
	_snwprintf(strbuf, sizeof strbuf, L"Puce SiRF configurée");
	SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) strbuf);	
	if (GetMessageW(&msg, NULL, 0, 0)) {
	    TranslateMessage(&msg);
	    DispatchMessageW(&msg);
	}
#endif	
	return TRUE;        
}

static BOOL parse_nmea_messages(HANDLE hcom)
{
	char serbuf[8192];
	DWORD readen;
	char *serbuf_p = serbuf;
	char sentence[4096];
	char *tok;
	size_t sentence_len;
	NmeaSentence *scanned_nmea_sentence;
	char *keyword;
	char *sep;

	if (ReadFile(hcom, serbuf, sizeof serbuf - 1U, &readen, NULL) == FALSE) {
		return FALSE;
	}
	serbuf[readen] = 0;
	gpstatus.initialized = TRUE;
	
#if DEBUG
	{
		FILE *fp = fopen("\\Evadeo\\debug.log", "at");
		fprintf(fp, "%s", serbuf);
		fclose(fp);
	}
#endif	
	
	while (*serbuf_p != 0) {
		tok = strchr(serbuf_p, '\r');
		if (tok == NULL) {
			sentence_len = strlen(serbuf_p);
		} else {
			sentence_len = (size_t) (tok - serbuf);
		}
		if (sentence_len > sizeof sentence) {
			goto next;
		}
		if (tok != NULL) {
			*tok = 0;
		}
		memcpy(sentence, serbuf_p, sentence_len + 1);
		scanned_nmea_sentence = nmea_sentences;
		if ((sep = strrchr(sentence, '*')) != NULL) {
			*sep = 0;
		}
#if 0		
		{
			FILE *fp = fopen("\\Evadeo\\position.log", "a");
			fprintf(fp, "[%s]\n", sentence);
			fclose(fp);
		}
#endif		
		keyword = tokenize(sentence, ',');
		do {
			if (strcasecmp(scanned_nmea_sentence->keyword, keyword) == 0) {
				scanned_nmea_sentence->callback(sentence);
				break;
			}
			scanned_nmea_sentence++;
		} while (scanned_nmea_sentence->keyword != NULL);
		next:
		if (tok == NULL) {
			break;
		}
		if (tok[1] == '\n') {
			tok++;
		}
		serbuf_p = tok + 1;
	}
	return TRUE;
}

LRESULT wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HDC hdc = NULL;
	static HDC bmap_dc = NULL;
	static HBITMAP hbmap = NULL;
	static WORD width, height;
	static RECT rect;
	PAINTSTRUCT ps;
	static LOGFONT lf;
	static HFONT font;
	static HFONT location_font;
	static HFONT oldfont = NULL;
	const COLORREF bg_color = 0x000000;	
	const COLORREF font_color = 0x7744bb;
	const COLORREF font_location_color = 0xff88cc;
	WCHAR strbuf[4000];
	Coord xlon = 0, xlat = 0;
	Coord xlon_diff, xlat_diff;
	int xaltitude = 0;
	int xaltitude_diff;
	MinDecCoord lon, lat;
	static const WCHAR *turner[] = {
		L" ... |",
		L" ... /",
		L" ... -",
		L" ... \\",
		NULL
	};
	static const WCHAR **turnerpnt = turner;	
	
    switch(msg){ 
    case WM_CREATE:
	hdc = GetDC(hWnd);
	bmap_dc = CreateCompatibleDC(hdc);
	GetClientRect(hWnd, &rect);
	width = rect.right;
	height = rect.bottom;
	hbmap = CreateCompatibleBitmap(hdc, width, height);
	SelectObject(bmap_dc, hbmap);
	
	ZeroMemory(&lf, sizeof lf);
	
#if 0
	{
		CHOOSEFONT cf;
		
		ZeroMemory(&cf, sizeof cf);
		cf.lStructSize = sizeof cf;
		cf.hwndOwner = hWnd;
		cf.lpLogFont = &lf;
		cf.rgbColors = 0;
		cf.Flags = CF_SCREENFONTS;
		ChooseFont(&cf);		
	}
#else	
	lf.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
	_snwprintf(lf.lfFaceName, sizeof lf.lfFaceName, L"%s", L"Arial");
	lf.lfWeight = FW_NORMAL;
	lf.lfHeight = 24;	
#endif	
	lf.lfQuality = ANTIALIASED_QUALITY;
	font = CreateFontIndirect(&lf);	
	oldfont = SelectObject(bmap_dc, font);	
	
	lf.lfWidth = 16;
	lf.lfHeight = 96;
	location_font = CreateFontIndirect(&lf);

#if 0
	SelectObject(bmap_dc, CreateSolidBrush(0xaaffcc)); // XXX - leak
#else
	SelectObject(bmap_dc, GetStockObject(HOLLOW_BRUSH)); // XXX - leak
#endif	
	
	ReleaseDC(hWnd, hdc);
	return 0;
	
    case WM_CLOSE:
	PostQuitMessage(0);

//	DestroyWindow(hWnd);
	break;
	
    case WM_DESTROY:
#if 0
	if (oldfont != NULL) {
		SelectObject(bmap_dc, oldfont);
		oldfont = NULL;
	}
	DeleteObject(font);
	DeleteObject(hbmap);
	DeleteDC(bmap_dc);
#endif	
	PostQuitMessage(0);
	break;
	
	case WM_ERASEBKGND:
	return 1;
	
	case WM_PAINT:
	hdc = BeginPaint(hWnd, &ps);
	PatBlt(bmap_dc, 0, 0, width, height, BLACKNESS);
	SetBkColor(bmap_dc, bg_color);
	SetTextColor(bmap_dc, font_color);
	SelectObject(bmap_dc, font);
	{
		const WCHAR *fix_quality_s;
		const WCHAR *tridifix_s;
		const WCHAR *tow_s;
		WCHAR dgps_details[1000];
		
		if (gpstatus.useful_sat_count > gpstatus.tracked_sat_count) {
			gpstatus.tracked_sat_count = gpstatus.useful_sat_count;
		}
		switch (gpstatus.fix_quality) {
			case FQ_INVALID:
			fix_quality_s = *turnerpnt++;
			if (*turnerpnt == NULL) {
				turnerpnt = turner;
			}
			break;
		 	case FQ_GPS:
			fix_quality_s = L"- GPS"; break;
			case FQ_DGPS:
			if (gpstatus.dgps_station_id > 0) {
				_snwprintf(dgps_details, sizeof dgps_details, L"+ EGNOS n°%d", gpstatus.dgps_station_id);
			} else {
				_snwprintf(dgps_details, sizeof dgps_details, L"+ EGNOS");
			}
			fix_quality_s = dgps_details;
			break;
			case FQ_PPS:
			fix_quality_s = L"- PPS"; break;
			case FQ_RTK:
			fix_quality_s = L"- RTK"; break;
			case FQ_FLOAT:
			fix_quality_s = L"- flottant"; break;
			case FQ_ESTIMATED:
			fix_quality_s = L"- estimation"; break;
			case FQ_MANUAL:
			fix_quality_s = L"- manuel"; break;
			case FQ_SIMULATION:
			fix_quality_s = L"- simulation"; break;
			default:
			fix_quality_s = L"";
		}
		switch (gpstatus.tridifix) {
			case TDFX_NOFIX:
			if (gpstatus.fix_quality == FQ_INVALID) {
				if (gpstatus.useful_sat_count > 0) {
					tridifix_s = L"instable";
				} else if (gpstatus.tracked_sat_count > 0) {
					tridifix_s = L"signal brouillé";
				} else {
					tridifix_s = L"pas de fix";
				}
			} else {
				tridifix_s = L"approximatif";
			}
			break;
			case TDFX_2D:
			tridifix_s = L"2D"; break;
			case TDFX_3D:
			tridifix_s = L"3D"; break;
			default:
			tridifix_s = L"initialisation";
		}
		switch (gpstatus.time_valid_flag) {
			case 0:
			tow_s = L"Date inconnue"; break;
			case 1:
			tow_s = L"Heure inconnue"; break;
			case 2:
			tow_s = L"Heure OK"; break;
			case 3:
			tow_s = L"Heure et date OK"; break;
			default:
			tow_s = L"OK"; break;
		}
		if (gpstatus.useful_sat_count <= 0 || gpstatus.tracked_sat_count <= 0) {
			gpstatus.lon = gpstatus.lat = 0;
		}
		if ((gpstatus.tridifix != TDFX_NOFIX || gpstatus.fix_quality != FQ_INVALID) &&
			(gpstatus.lat != 0 || gpstatus.lon != 0) &&
			gpstatus.useful_sat_count > 0 &&
			gpstatus.hdop != 0.0 && gpstatus.hdop < 7.5) {
			double weight;
				
			gpstatus.avg_total_altitude += gpstatus.altitude;
			gpstatus.avg_altitude_counts++;
			xaltitude = gpstatus.avg_total_altitude / gpstatus.avg_altitude_counts;

			weight = gpstatus.hdop * gpstatus.hdop;
			gpstatus.avg_total_lat += gpstatus.lat / weight;
			gpstatus.avg_total_lon += gpstatus.lon / weight;
				
			gpstatus.avg_lonlat_counts += weight;
				
			xlat = gpstatus.avg_total_lat / gpstatus.avg_lonlat_counts;
			xlon = gpstatus.avg_total_lon / gpstatus.avg_lonlat_counts;
		} else if (gpstatus.avg_lonlat_counts > 0) {
			xlat = gpstatus.avg_total_lat / gpstatus.avg_lonlat_counts;
			xlon = gpstatus.avg_total_lon / gpstatus.avg_lonlat_counts;
		} else {
			xaltitude = gpstatus.altitude;			
			if (gpstatus.lat == 0 && gpstatus.lon == 0 && gpstatus.avg_lonlat_counts > 0) {
				xlat = gpstatus.avg_total_lat / gpstatus.avg_lonlat_counts;
				xlon = gpstatus.avg_total_lon / gpstatus.avg_lonlat_counts;
			} else {
				xlat = gpstatus.lat;
				xlon = gpstatus.lon;
			}
		}
		if (gpstatus.lat == 0 && gpstatus.lon == 0 && gpstatus.avg_lonlat_counts > 0) {
			xlat = gpstatus.avg_total_lat / gpstatus.avg_lonlat_counts;
			xlon = gpstatus.avg_total_lon / gpstatus.avg_lonlat_counts;
		} else if (gpstatus.lat != 0 || gpstatus.lon != 0) {
			xlon_diff = xlon - gpstatus.lon;
			xlat_diff = xlat - gpstatus.lat;
			if (xlon_diff < -0.050 || xlon_diff > 0.050 ||
				xlat_diff < -0.050 || xlat_diff > 0.050) {
				gpstatus.avg_total_lon = 0;
				gpstatus.avg_total_lat = 0;
				gpstatus.avg_lonlat_counts = 0;
				xlat = gpstatus.lat;
				xlon = gpstatus.lon;		
			}
		}
		xaltitude_diff = xaltitude - gpstatus.altitude;
		if (xaltitude_diff < -50 || xaltitude_diff > 50) {
			gpstatus.avg_total_altitude = 0;
			gpstatus.avg_altitude_counts = 0;
			xaltitude = gpstatus.altitude;		
		}
		if (gpstatus.found_ephemeris >= 0) {
			_snwprintf(strbuf, sizeof strbuf,
				L"Type de fix : %s %s\r\n"
				L"Satellites suivis : %d - utilisés : %d\r\n"
				L"Dilution horizontale : %.2f\r\n"
				L"Instant fix : %d %% - %s\r\n"
				L"Signal : %d (%s%.3f dB) - Alt : %d m",
				tridifix_s, fix_quality_s,
				gpstatus.tracked_sat_count,
				gpstatus.useful_sat_count,
				gpstatus.hdop,
				gpstatus.found_ephemeris * 100 / EPHEMERIS_NUMBER, tow_s,
				gpstatus.decibels <= 0.0 ? 0 : ((int) round(pow(10.0, gpstatus.decibels / 10.0))),
				gpstatus.decibels <= 0.0 ? L"< " : L"", gpstatus.decibels,
				xaltitude);
		} else {
			_snwprintf(strbuf, sizeof strbuf,
				L"Type de fix : %s %s\r\n"
				L"Satellites suivis : %d - utilisés : %d\r\n"
				L"Dilution horizontale : %.2f\r\n"
				L"Signal : %d (%s%.3f dB) - Alt : %d m",
				tridifix_s, fix_quality_s,
				gpstatus.tracked_sat_count,
				gpstatus.useful_sat_count,
				gpstatus.hdop,
				gpstatus.decibels <= 0.0 ? 0 : ((int) round(pow(10.0, gpstatus.decibels / 10.0))),
				   gpstatus.decibels <= 0.0 ? L"< " : L"", gpstatus.decibels,
				xaltitude);			
		}
		DrawText(bmap_dc, strbuf, -1, &rect, DT_LEFT | DT_TOP /* | DT_VCENTER | DT_SINGLELINE */);
	}
	SetTextColor(bmap_dc, font_location_color);	
	SelectObject(bmap_dc, location_font);
	lat = coord_nmea_to_mindec(xlat);
	lon = coord_nmea_to_mindec(xlon);
	_snwprintf(strbuf, sizeof strbuf, L"%d°%02d.%03d %s  %d°%02d.%03d %s",
		lat.deg, lat.min, lat.dec, gpstatus.lat_dir, lon.deg, lon.min, lon.dec, gpstatus.lon_dir);
	DrawText(bmap_dc, strbuf, -1, &rect, DT_CENTER | DT_BOTTOM /* | DT_VCENTER | DT_SINGLELINE */);	
	
	if (GRIBOUILLI && gpstatus.hdop != 0 && (xlat != 0 || xlon != 0) && gpstatus.fix_quality != FQ_INVALID) {
		static LastPos last_pos[150];
		static Coord min_lat = 1000000.0, min_lon = 1000000.0;
		static Coord max_lat = -1000000.0, max_lon = -1000000.0;
		double dx, dy;
		double odx, ody;
		double xxlon, xxlat;
		int p;

#if 0
		xlon = rand() % 1000;
		xlat = rand() % 1000;
#endif	
		if (1) {
			memmove(&last_pos[1], &last_pos[0], sizeof last_pos - sizeof last_pos[0]);
		}
		last_pos[0].lat = xlat;
		last_pos[0].lon = xlon;
		last_pos[0].hdop = gpstatus.hdop;
		
		min_lat = 1000000.0;
		min_lon = 1000000.0;
		max_lat = -1000000.0;
		max_lon = -1000000.0;
		
		p = sizeof last_pos / sizeof last_pos[0];
		do {
			p--;
			if (last_pos[p].hdop == 0) {
				continue;
			}
			xxlon = last_pos[p].lon;
			xxlat = last_pos[p].lat;
			if (xxlon < min_lon) min_lon = xxlon;
			if (xxlat < min_lat) min_lat = xxlat;
			if (xxlon > max_lon) max_lon = xxlon;
			if (xxlat > max_lat) max_lat = xxlat;			
		} while (p > 0);
		if (max_lat == min_lat) {
			min_lat -= 0.001;
			max_lat += 0.001;
		}
		if (max_lon == min_lon) {
			min_lon -= 0.001;
			max_lon += 0.001;
		}
		odx = -1;
		ody = -1;
		p = sizeof last_pos / sizeof last_pos[0];
		do {
			double ray;
			HPEN pen;
			COLORREF color;

			p--;
			if (last_pos[p].hdop == 0) {
				continue;
			}
			xxlon = last_pos[p].lon;
			xxlat = last_pos[p].lat;			
			ray = round(10.0 / (double) (p + 1) + 1.0);
			dx = round((xxlat - min_lat) * (double) width / (max_lat - min_lat));			
			dy = round((xxlon - min_lon) * (double) height / (max_lon - min_lon));
			
			if (last_pos[p].hdop <= 1.0) {
				color = 0x44ff44;
			} else {
				color = (COLORREF) round((double) 0xffffff / last_pos[p].hdop);
				color <<= 8;
			}
			pen = CreatePen(PS_SOLID, 1, color);
			SelectObject(bmap_dc, pen);
			if (odx != -1) {
				MoveToEx(bmap_dc, odx, ody, NULL);
				LineTo(bmap_dc, dx, dy);
			}
			odx = dx;
			ody = dy;			
			Ellipse(bmap_dc, dx - ray, dy - ray, dx + ray, dy + ray);
			DeleteObject(pen);
		} while (p > 0);
	}
	
	BitBlt(hdc, 0, 0, width, height, bmap_dc, 0, 0, SRCCOPY);
	EndPaint(hWnd, &ps);
	return 1;
	
	case WM_TIMER:
	parse_nmea_messages(global_hcom);
	timer = SetTimer(hWnd, 1, FRAME_RATE, NULL);		
	InvalidateRect(hWnd, NULL, FALSE);
	return 1;	
	
    default: 
	return DefWindowProc(hWnd,msg,wParam,lParam); 
    } 
    return 0; 
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,
	    int nCmdShow)
{   
	WNDCLASS wnd_cls;
	HWND h_wnd;
    MSG msg;
	HANDLE hcom;
#ifdef USHUAIA
	const WCHAR *sirf_ports[] = { L"COM2:", L"COM8:", L"COM7:", L"COM1:", NULL };
	const WCHAR *comid_ports[] = { L"COM2:", L"COM1:", L"COM8:", L"COM7:", NULL };
#else
	const WCHAR *sirf_ports[] = { L"COM8:", L"COM7:", L"COM1:", NULL };
	const WCHAR *comid_ports[] = { L"COM1:", L"COM8:", L"COM7:", NULL };
#endif	

	(void) hInstance;
	(void) hPrevInstance;
	(void) lpCmdLine;
	(void) nCmdShow;

	ZeroMemory(&gpstatus, sizeof gpstatus);
	
	gpstatus.avg_total_lat = gpstatus.avg_total_lon = 0;
	gpstatus.avg_lonlat_counts = 0;
	gpstatus.avg_total_altitude = 0;
	gpstatus.avg_altitude_counts = 0;
	gpstatus.found_ephemeris = -1;
	gpstatus.initialized = FALSE;
	ZeroMemory(&wnd_cls, sizeof wnd_cls);
    wnd_cls.style = CS_HREDRAW | CS_VREDRAW;
    wnd_cls.hInstance = hInstance;
    wnd_cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd_cls.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wnd_cls.lpszClassName = L"Evadeo Affix";
    wnd_cls.lpfnWndProc = (WNDPROC) wnd_proc;

   	RegisterClass(&wnd_cls);
    h_wnd = FindWindow(L"Evadeo Affix", L"Evadeo Affix pour www.evadeiste.fr");
    if (h_wnd) {
		SetForegroundWindow((HWND) ((ULONG) h_wnd | 0x0001));
		return 0;
    }
	hide_taskbar();
    h_wnd = CreateWindowEx(WS_EX_NODRAG, L"Evadeo Affix", L"Evadeo Affix v0.91 pour www.evadeiste.fr",
		WS_CAPTION | WS_SYSMENU | WS_EX_TOPMOST,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);	
    if (!h_wnd) {
		MessageBox(NULL, L"Impossible de creer la fenetre",
		   TEXT("Evadeo Affix"), MB_OK);
		restore_taskbar();
		
		return 1;
    }
#if 1
    h_test = CreateWindow(WC_STATIC, L"Evadeo Affix",
			  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_EX_CONTROLPARENT,
			  0, 100, 480, 20, h_wnd, NULL, hInstance, NULL);
#endif			
    ShowWindow(h_wnd, SW_SHOW);
    UpdateWindow(h_wnd);
    ForcePageout();
#if 1
    SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) L"Démarrage en cours...");
	if (GetMessageW(&msg, NULL, 0, 0)) {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}
#endif	
	if (setup_sirf(sirf_ports) == FALSE) {
		DestroyWindow(h_wnd);
		UnregisterClass(wnd_cls.lpszClassName, NULL);
		restore_taskbar();
		
	    return 1;
	}
	
	Sleep(1500);
		
	hcom = open_port(comid_ports, TRUE);
	if (hcom == INVALID_HANDLE_VALUE) {
		MessageBox(NULL, L"Impossible d'acceder au port du pilote intermediaire",
		       TEXT("Evadeo Affix"), MB_OK);	
		DestroyWindow(h_wnd);
		UnregisterClass(wnd_cls.lpszClassName, NULL);
		restore_taskbar();		
		return 1;
	}
	Sleep(1500);
    SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) L"Chargement des éphémérides...");
	if (GetMessageW(&msg, NULL, 0, 0)) {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}
	CloseHandle(hcom);
	
    SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) L"Tracking...");
	if (GetMessageW(&msg, NULL, 0, 0)) {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}
	Sleep(1500);
	hcom = open_port(sirf_ports, TRUE);
	if (hcom == INVALID_HANDLE_VALUE) {
		MessageBox(NULL, L"Impossible d'acceder au port SiRF (tracking)",
		       TEXT("Evadeo Affix"), MB_OK);	
		DestroyWindow(h_wnd);
		UnregisterClass(wnd_cls.lpszClassName, NULL);
		restore_taskbar();		
		return 1;
	}
#if 1
	DestroyWindow(h_test);
#endif	
	global_hcom = hcom;
	timer = SetTimer(h_wnd, 1, FRAME_RATE, NULL);	
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	PurgeComm(hcom, PURGE_RXCLEAR);	
	CloseHandle(hcom);

	Sleep(1000);
	
	hcom = open_port(comid_ports, TRUE);
	if (hcom == INVALID_HANDLE_VALUE) {
		MessageBox(NULL, L"Impossible d'acceder au port du pilote intermediaire",
		       TEXT("Evadeo Affix"), MB_OK);	
		DestroyWindow(h_wnd);
		UnregisterClass(wnd_cls.lpszClassName, NULL);
		restore_taskbar();		
		return 1;
	}
	DestroyWindow(h_wnd);
	PurgeComm(hcom, PURGE_RXCLEAR);
	PurgeComm(hcom, PURGE_TXCLEAR);
	Sleep(500);
	PurgeComm(hcom, PURGE_RXCLEAR);
	CloseHandle(hcom);
	UnregisterClass(wnd_cls.lpszClassName, NULL);
	restore_taskbar();
	
    return 0;
}
