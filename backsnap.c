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

#include "png.h"

#define BMP 0

#if BMP
BOOL SaveBitmap(HDC hdc, HBITMAP hBitmap, BITMAPINFO bmi,
	unsigned char *bits, const char *szPath)
{
	FILE *fp;
	BITMAP Bm;
	BITMAPINFO BitInfo;
	BITMAPFILEHEADER BmHdr;

	fp = fopen(szPath, "wb");    
	if (fp == NULL) {
		return FALSE;
	}
	ZeroMemory(&BitInfo, sizeof BitInfo);
	ZeroMemory(&BmHdr, sizeof BmHdr);
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;
	BitInfo.bmiHeader = bmi.bmiHeader;
	BitInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	BitInfo.bmiHeader.biSizeImage =
		BitInfo.bmiHeader.biHeight * BitInfo.bmiHeader.biWidth * 3;
	BitInfo.bmiHeader.biBitCount = 24;
	Bm.bmWidth  = BitInfo.bmiHeader.biWidth;
	Bm.bmHeight = BitInfo.bmiHeader.biHeight;
	BmHdr.bfType        = 0x4d42;   // 'BM' WINDOWS_BITMAP_SIGNATURE
	BmHdr.bfSize        = (((3 * Bm.bmWidth + 3) & ~3) * Bm.bmHeight) 
		+ sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	BmHdr.bfReserved1    = BmHdr.bfReserved2 = 0;
	BmHdr.bfOffBits      = (DWORD) sizeof(BITMAPFILEHEADER) 
		+ sizeof(BITMAPINFOHEADER);

	fwrite(&BmHdr, sizeof(BITMAPFILEHEADER), 1, fp);
	fwrite(&BitInfo.bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp);    
	fwrite(bits, 1, BitInfo.bmiHeader.biSizeImage, fp);
	fclose(fp);

	return TRUE;
}
#else
BOOL SaveBitmap(HDC hdc, HBITMAP hBitmap, BITMAPINFO bmi,
	unsigned char *bits, const char *szPath)
{
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned int width = bmi.bmiHeader.biWidth;
	unsigned int height = bmi.bmiHeader.biHeight;
	unsigned int k;
	png_bytep row_pointers[height];
	png_text text_ptr[1];
	unsigned char *buf = bits;

	fp = fopen(szPath, "wb");    
	if (fp == NULL) {
		return FALSE;
	}
	for (k = 0; k < height; k++) {
		row_pointers[height - 1 - k] = buf + k * width * 3;
	}
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL);
	if (png_ptr == NULL) {
		fclose(fp);
		unlink(szPath);
		return FALSE;
	}     
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fp);
		unlink(szPath);
		return FALSE;
	}
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fp);
		unlink(szPath);
		return FALSE;
	}
	png_init_io(png_ptr, fp);
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	png_set_bgr(png_ptr);
	png_set_IHDR(png_ptr, info_ptr, width, height, 8,
		PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	memset(text_ptr, 0, sizeof *text_ptr);
	text_ptr[0].key = "Generator";
	text_ptr[0].text = "Grabbed by Backsnap, by Frank DENIS";
	text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
# ifdef PNG_iTXt_SUPPORTED
	text_ptr[0].lang = NULL;
# endif
	png_set_text(png_ptr, info_ptr, text_ptr,
		sizeof text_ptr / sizeof text_ptr[0]);
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);
	if (fclose(fp) != 0) {
		unlink(szPath);	
		return FALSE;
	}
	return TRUE;
}
#endif


BOOL snapshot(const char *filename)
{
	HDC scr;
	HDC comp; 
	HBITMAP hbmscreen;
	HWND hwnd;
	DWORD width, height;
	BITMAPINFO bmi;
	void *ppvbits;

	scr = CreateDC(L"DISPLAY", NULL, NULL, NULL);
	comp = CreateCompatibleDC(scr);
	width = GetDeviceCaps(scr, HORZRES);
	height = GetDeviceCaps(scr, VERTRES);
	ZeroMemory(&bmi, sizeof bmi);
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;    
	bmi.bmiHeader.biHeight = height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;
	hbmscreen = CreateDIBSection(comp, &bmi, DIB_RGB_COLORS, &ppvbits, NULL, 0);
	if (hbmscreen == 0) {
		MessageBox(0, L"hbmscreen = 0", L"Erreur...", MB_OK);
		return FALSE;
	}
	if (!SelectObject(comp, hbmscreen)) {
		MessageBox(0, L"SelectObject foire", L"Erreur...", MB_OK);
		return FALSE;	
	}
	hwnd = GetForegroundWindow();
//    ShowWindow(hwnd, SW_HIDE);
	if (!BitBlt(comp, 0, 0, width, height, scr, 0, 0, SRCCOPY)) {
		MessageBox(0, L"BitBlt foire", L"Erreur...", MB_OK);
		return FALSE;	
	}
//    ShowWindow(hwnd, SW_SHOW);
	SaveBitmap(comp, hbmscreen, bmi, ppvbits, filename);
	DeleteDC(comp);
//    ReleaseDC(0, scr);

	return TRUE;
}

#if 0
BOOL snapshotkey(void)
{
	INPUT input[3];
	int c;

	ZeroMemory(&input, sizeof input);

	c = 120;
	while (c < 140) {
		input[0].type = INPUT_KEYBOARD;
		input[0].ki.wVk = 0x53;
		input[0].ki.dwFlags = 0;
		input[1].type = INPUT_KEYBOARD;
		input[1].ki.wVk = 0x53;
		input[1].ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(2U, input, sizeof *input);
		break;
		c++;
	}
	return TRUE;

	input[0].type = INPUT_KEYBOARD;
	input[0].ki.wVk = VK_LWIN;
	input[0].ki.wVk = VK_LSHIFT;
	input[0].ki.dwFlags = 0;

	input[1].type = INPUT_KEYBOARD;
	input[1].ki.wVk = 105; // 9
	input[1].ki.dwFlags = 0;

	input[2].type = INPUT_KEYBOARD;
	input[2].ki.wVk = VK_LWIN;
	input[2].ki.wVk = VK_LSHIFT;
	input[2].ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(3U, input, sizeof *input);

	return TRUE;
}
#endif

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine,
	int nCmdShow)
{
	char filename[MAX_PATH];

	MessageBox(0, L"L'outil capture d'ecran est en route !\r\n"
		"Si un dossier 'CAPTURES' existe a la racine de la carte SD, "
		"des captures d'ecran y seront crees toutes les 30 secondes.",
		L"Evadeo Backsnap v1.1, par Axey pour www.evadeiste.fr", MB_OK);
//    mkdir("\\EVADEO\\CAPTURES");
	for(;;) {
		Sleep(30000);
#if BMP
		_snprintf(filename, sizeof filename, "\\EVADEO\\CAPTURES\\capture_%lu.bmp",
			time(NULL));
#else
		_snprintf(filename, sizeof filename, "\\EVADEO\\CAPTURES\\capture_%lu.png",
			time(NULL));
#endif
		snapshot(filename);
	}
	return 0;
}
