all:	TwonavEvadeo.exe evadeo-backsnap.exe evadeo-bench.exe \
	evadeo-boost.exe evadeo-rotate.exe evadeo-taskbar.exe \
	evadeo-refresh.exe evadeo-memwatch.exe executabilitycheck.exe \
	evadeo-tweak.exe evadeo-dlltest.exe evadeo-affix.exe \
	evadeo-statnav.exe evadeo-screenoff.exe evadeo-test-screenoff.exe \
	ushuaia-affix.exe G407nVDLL.dll

TwonavEvadeo.exe:	evadeo-bootstrap.c Makefile
	arm-mingw32ce-gcc evadeo-bootstrap.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o TwonavEvadeo.exe -D_WIN32_WCE=0x0500 -Wno-unused -static \
	-lceshell -lm

	ls -l TwonavEvadeo.exe

evadeo-bench.exe:	evadeo-bench.c Makefile
	arm-mingw32ce-gcc evadeo-bench.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-bench.exe -D_WIN32_WCE=0x0500 -static
	ls -l evadeo-bench.exe

evadeo-boost.exe:	evadeo-boost.c Makefile
	arm-mingw32ce-gcc evadeo-boost.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-boost.exe -D_WIN32_WCE=0x0500
	ls -l evadeo-boost.exe

evadeo-backsnap.exe:	backsnap.c Makefile
	arm-mingw32ce-gcc backsnap.c -o evadeo-backsnap.exe -Wall -W -s -Os -mcpu=arm920t \
	-fomit-frame-pointer \
	-IPNG/libpng-1.2.34 -LPNG/libpng-1.2.34 -lpng \
	-IZ/libz -LZ/libz -lz -lm -D_WIN32_WCE=0x0500 -static
	
	ls -l evadeo-backsnap.exe

evadeo-rotate.exe:	evadeo-rotate.c Makefile
	arm-mingw32ce-gcc evadeo-rotate.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-rotate.exe -D_WIN32_WCE=0x0500
	ls -l evadeo-rotate.exe

evadeo-taskbar.exe:	evadeo-taskbar.c Makefile
	arm-mingw32ce-gcc evadeo-taskbar.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-taskbar.exe -D_WIN32_WCE=0x0500
	ls -l evadeo-taskbar.exe

evadeo-refresh.exe:	evadeo-refresh.c Makefile
	arm-mingw32ce-gcc evadeo-refresh.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-refresh.exe -D_WIN32_WCE=0x0500
	ls -l evadeo-refresh.exe

evadeo-memwatch.exe:	evadeo-memwatch.c Makefile
	arm-mingw32ce-gcc evadeo-memwatch.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-memwatch.exe -D_WIN32_WCE=0x0500 -static
	ls -l evadeo-memwatch.exe

evadeo-tweak.exe:	evadeo-tweak.c Makefile
	arm-mingw32ce-gcc evadeo-tweak.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-tweak.exe -static -lm -D_WIN32_WCE=0x0500
	ls -l evadeo-tweak.exe

evadeo-dlltest.exe:	evadeo-dlltest.c Makefile
	arm-mingw32ce-gcc evadeo-dlltest.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-dlltest.exe -static -D_WIN32_WCE=0x0500
	ls -l evadeo-dlltest.exe

evadeo-affix.exe:	evadeo-affix.c Makefile
	arm-mingw32ce-gcc evadeo-affix.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-affix.exe -static -D_WIN32_WCE=0x0500 # -lcommdlg
	ls -l evadeo-affix.exe

ushuaia-affix.exe:	evadeo-affix.c Makefile
	arm-mingw32ce-gcc evadeo-affix.c -Wall -W -s -Os -fomit-frame-pointer \
	-o ushuaia-affix.exe -static -D_WIN32_WCE=0x0400 -DUSHUAIA=1
	ls -l ushuaia-affix.exe

evadeo-statnav.exe:	evadeo-statnav.c Makefile
	arm-mingw32ce-gcc evadeo-statnav.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-statnav.exe -static -D_WIN32_WCE=0x0500
	ls -l evadeo-statnav.exe

evadeo-screenoff.exe:	evadeo-screenoff.c Makefile
	arm-mingw32ce-gcc evadeo-screenoff.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-screenoff.exe -static -D_WIN32_WCE=0x0500
	ls -l evadeo-screenoff.exe

evadeo-test-screenoff.exe:	evadeo-test-screenoff.c Makefile
	arm-mingw32ce-gcc evadeo-test-screenoff.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o evadeo-test-screenoff.exe -static -D_WIN32_WCE=0x0500
	ls -l evadeo-test-screenoff.exe

executabilitycheck.exe:	executabilitycheck.c Makefile
	arm-mingw32ce-gcc executabilitycheck.c -Wall -W -s -Os -mcpu=arm920t -fomit-frame-pointer \
	-o executabilitycheck.exe -D_WIN32_WCE=0x0500
	ls -l executabilitycheck.exe

G407nVDLL.dll:	evadeo-vdll.c Makefile
	arm-mingw32ce-gcc evadeo-vdll.c -Wall -W -s -Os -mcpu=arm920t \
	-shared -o G407nVDLL.dll -D_WIN32_WCE=0x0500 -Wno-unused -static

	ls -l G407nVDLL.dll

