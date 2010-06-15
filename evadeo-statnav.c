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

#define STATNAV 1

#define EPHEMERIS_NUMBER 32
#define POWER_MASK 20
#define FRAME_RATE (1000 / 2)

extern VOID ForcePageout(void);

static HWND h_test;
static UINT timer = 0;
static HANDLE global_hcom;

typedef struct MinDecCoord_ {
  int deg;
  int min;
  int dec;
} MinDecCoord;

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

typedef double Coord;

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
	
	unsigned int decibels;
	unsigned int masked_decibels;
	
	Coord avg_total_lat;
	Coord avg_total_lon;
	long avg_lonlat_counts;
	
	long int avg_total_altitude;
	long avg_altitude_counts;
} GPStatus;

static GPStatus gpstatus;

int nmea_cb_gpgga(char *sentence)
{  
	char *str;
	WCHAR *wstr;
	
	tokenize_int(sentence); /* fix_date */
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

int nmea_cb_psrf151(char *sentence)
{
	unsigned int ephemeris;
	int missing_ephemeris = 0;
	int bits = 32;
	
	gpstatus.time_valid_flag = tokenize_int(sentence);
	tokenize_int(sentence); /* week number */
	gpstatus.tow = tokenize_int(sentence);
	ephemeris = (unsigned int) tokenize_int(sentence);
	
	do {
		if ((ephemeris & 31) != 0) {
			missing_ephemeris++;
		}
		ephemeris <<= 1;
	} while (--bits != 0);
	if (missing_ephemeris > EPHEMERIS_NUMBER) {
		missing_ephemeris = EPHEMERIS_NUMBER;
	}
	gpstatus.found_ephemeris = EPHEMERIS_NUMBER - missing_ephemeris;
	
	return 0;
}

int nmea_cb_gpgsv(char *sentence)
{
	static unsigned int decibels = 0U;
	static unsigned int masked_decibels = 0U;	
	int nb;
	int total_nb;
	int sats_per_sentence = 4;
	unsigned int snr;
	
	total_nb = tokenize_int(sentence); /* number of sentences */
	nb = tokenize_int(sentence);
	tokenize_int(sentence); /* number of stats in view */
	if (nb < 2) {
		decibels = 0U;
		masked_decibels = 0U;
	}
	do {
		tokenize_int(sentence); /* PRN */
		tokenize_int(sentence); /* elevation */
		tokenize_int(sentence); /* azimuth */
		snr = (unsigned int) tokenize_int(sentence);
		if (snr > 100) {
			snr = 100;
		}
		decibels += snr;
		if (snr > POWER_MASK) {
			masked_decibels += snr;
		}
	} while (--sats_per_sentence != 0);
	if (total_nb == nb) {
		gpstatus.decibels = decibels;
		gpstatus.masked_decibels = masked_decibels;
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
	{ NULL, (NmeaCallback) NULL }
};

static BOOL wait_any_data(HANDLE hcom)
{
	CHAR k[8192];
	DWORD readen;
	
	ReadFile(hcom, k, sizeof k - 1, &readen, NULL);
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

static HANDLE open_port(const WCHAR **ports_list)
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
			CloseHandle(hcom);
			hcom = INVALID_HANDLE_VALUE;
			continue;
		}
		dcb.DCBlength = sizeof dcb;
		dcb.BaudRate = 57600;
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
		PurgeComm(hcom, PURGE_RXCLEAR);
		PurgeComm(hcom, PURGE_TXCLEAR);		
		break;
	} while (*++port != NULL);
	
	return hcom;
}

static BOOL setup_sirf(const WCHAR **ports_list, BOOL statnav)
{ 
	WCHAR strbuf[1000];
	MSG msg;
	HANDLE hcom;
	
	hcom = open_port(ports_list);
	if (hcom == INVALID_HANDLE_VALUE) {
		MessageBox(NULL, L"Impossible d'acceder a la puce SiRF",
		       TEXT("Evadeo StatNav"), MB_OK);	
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
	
	send_nmea_command(hcom, "PSRF100,0,57600,8,1,0");
	Sleep(100);
	send_nmea_command(hcom, "PSRF100,0,57600,8,1,0");
	Sleep(100);
	send_nmea_command(hcom, "PSRF100,0,57600,8,1,0");
	Sleep(100);	
	
	{
		const unsigned char sc_nop[] = {
			0x00
		};
		send_sirf_command(hcom, sc_nop, sizeof sc_nop);
	}
	
	{
		const unsigned char sc_enable_sbas[] = {
			0x85,
			0x01,
			0x00, 0x00, 0x00, 0x00,
			0x00 };
		send_sirf_command(hcom, sc_enable_sbas, sizeof sc_enable_sbas);
	}
	
	{
		const unsigned char sc_enable_static_mode[] = {
			0x8F, 0x01
		};
		const unsigned char sc_disable_static_mode[] = {
			0x8F, 0x00
		};
		if (statnav == TRUE) {
			send_sirf_command(hcom, sc_enable_static_mode, sizeof sc_enable_static_mode);
		} else {
			send_sirf_command(hcom, sc_disable_static_mode, sizeof sc_disable_static_mode);
		}
	}
#if 1
	{
		const unsigned char sc_back_to_nmea[] = {
			0x81, 0x02, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01,
			0xE1, 0x00 /* 57600 */
		};
		send_sirf_command(hcom, sc_back_to_nmea, sizeof sc_back_to_nmea);
		send_sirf_command(hcom, sc_back_to_nmea, sizeof sc_back_to_nmea);
		send_sirf_command(hcom, sc_back_to_nmea, sizeof sc_back_to_nmea);				
	}
#endif				
		
	send_nmea_command(hcom, "PSRF103,0,0,0,1");
	send_nmea_command(hcom, "PSRF103,4,0,1,1");	
	send_nmea_command(hcom, "PSRF103,0,0,1,1");
	send_nmea_command(hcom, "PSRF103,2,0,1,1");
	send_nmea_command(hcom, "PSRF103,3,0,1,1");
	
	wait_any_data(hcom);	
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
	const COLORREF font_color = 0x6633aa;
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
	lf.lfQuality = ANTIALIASED_QUALITY;
	lf.lfWidth = 16;
	lf.lfHeight = 24;	
	font = CreateFontIndirect(&lf);	
	oldfont = SelectObject(bmap_dc, font);	
	
	lf.lfWidth = 20;
	lf.lfHeight = 96;
	location_font = CreateFontIndirect(&lf);
			
	ReleaseDC(hWnd, hdc);
	return 0;
	
    case WM_CLOSE:
	DestroyWindow(hWnd);
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
#if STATNAV
	PatBlt(hdc, 0, 0, width, height, WHITENESS);
	EndPaint(hWnd, &ps);
	return 1;
#endif	
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
			(gpstatus.lat != 0 || gpstatus.lon != 0)) {
			gpstatus.avg_total_altitude += gpstatus.altitude;
			gpstatus.avg_altitude_counts++;
			xaltitude = gpstatus.avg_total_altitude / gpstatus.avg_altitude_counts;

			gpstatus.avg_total_lat += gpstatus.lat;
			gpstatus.avg_total_lon += gpstatus.lon;
			gpstatus.avg_lonlat_counts++;
			xlat = gpstatus.avg_total_lat / gpstatus.avg_lonlat_counts;
			xlon = gpstatus.avg_total_lon / gpstatus.avg_lonlat_counts;
		} else if (gpstatus.avg_lonlat_counts > 0) {
			xaltitude = gpstatus.avg_total_altitude / gpstatus.avg_altitude_counts;

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
		_snwprintf(strbuf, sizeof strbuf,
			L"Type de fix : %s %s\r\n"
			L"Satellites suivis : %d - utilisés : %d\r\n"
#if 0			
			L"Altitude GPS : %d m (vdop = %.2f)\r\n"
#endif			
			L"Dilution horizontale : %.2f\r\n"
			L"Instant fix : %d %% - %s\r\n"
			L"Signal : %u dB",
			tridifix_s, fix_quality_s,
			gpstatus.tracked_sat_count,
			gpstatus.useful_sat_count,
#if 0			
			gpstatus.altitude, gpstatus.vdop,
#endif			
			gpstatus.hdop,
			gpstatus.found_ephemeris * 100 / EPHEMERIS_NUMBER, tow_s,
			gpstatus.decibels);
		DrawText(bmap_dc, strbuf, -1, &rect, DT_LEFT | DT_TOP /* | DT_VCENTER | DT_SINGLELINE */);
	}
	SetTextColor(bmap_dc, font_location_color);	
	SelectObject(bmap_dc, location_font);
	lat = coord_nmea_to_mindec(xlat);
	lon = coord_nmea_to_mindec(xlon);
	_snwprintf(strbuf, sizeof strbuf, L"%d°%02d.%03d %s  %d°%02d.%03d %s",
		lat.deg, lat.min, lat.dec, gpstatus.lat_dir, lon.deg, lon.min, lon.dec, gpstatus.lon_dir);
	DrawText(bmap_dc, strbuf, -1, &rect, DT_CENTER | DT_BOTTOM /* | DT_VCENTER | DT_SINGLELINE */);	
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
	const WCHAR *sirf_ports[] = { L"COM8:", L"COM7:", L"COM1:", NULL };
	const WCHAR *comid_ports[] = { L"COM1:", L"COM8:", L"COM7:", NULL };

#if STATNAV
	BOOL statnav = FALSE;
	int choice;
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
	gpstatus.initialized = FALSE;
	ZeroMemory(&wnd_cls, sizeof wnd_cls);
    wnd_cls.style = CS_HREDRAW | CS_VREDRAW;
    wnd_cls.hInstance = hInstance;
    wnd_cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    wnd_cls.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wnd_cls.lpszClassName = L"Evadeo StatNav";
    wnd_cls.lpfnWndProc = (WNDPROC) wnd_proc;

   	RegisterClass(&wnd_cls);
    h_wnd = FindWindow(L"Evadeo StatNav", L"Evadeo StatNav pour www.evadeiste.fr");
    if (h_wnd) {
		SetForegroundWindow((HWND) ((ULONG) h_wnd | 0x0001));
		return 0;
    }
    h_wnd = CreateWindowEx(WS_EX_NODRAG, L"Evadeo StatNav", L"Evadeo StatNav v1.0 pour www.evadeiste.fr",
		WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);	
    if (!h_wnd) {
		MessageBox(NULL, L"Impossible de creer la fenetre",
		   TEXT("Evadeo StatNav"), MB_OK);	
		return 1;
    }
#if 1
    h_test = CreateWindow(WC_STATIC, L"Evadeo StatNav",
			  WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_EX_CONTROLPARENT,
			  0, 100, 480, 20, h_wnd, NULL, hInstance, NULL);
#endif			
    ShowWindow(h_wnd, SW_SHOW);
    UpdateWindow(h_wnd);
    ForcePageout();
#if 1
    SendMessage(h_test, WM_SETTEXT, 0, (LPARAM) L"Configuration de la static navigation...");
	if (GetMessageW(&msg, NULL, 0, 0)) {
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}
#endif
#if STATNAV
	choice = MessageBox(h_wnd,
		L"Activer la \"static navigation\" (latence augmentée, mais mais position fixe à l'arrêt) ?",
		L"Evadeo StatNav",	MB_YESNO | MB_ICONQUESTION);
	if (choice == IDYES) {
		statnav = TRUE;
	}
#endif
	if (setup_sirf(sirf_ports, statnav) == FALSE) {
		DestroyWindow(h_wnd);
		UnregisterClass(wnd_cls.lpszClassName, NULL);

	    return 1;
	}
#if STATNAV == 0
	Sleep(1000);
#endif	
#if 1
	DestroyWindow(h_test);
#endif
	DestroyWindow(h_wnd);
	UnregisterClass(wnd_cls.lpszClassName, NULL);

    return 0;
}
