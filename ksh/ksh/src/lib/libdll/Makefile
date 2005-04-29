:PACKAGE: ast

LICENSE = since=1997,author=gsf

CCFLAGS = $(CC.OPTIMIZE) $(CC.DLL)

dll 1.0 :LIBRARY: dlfcn.c dllfind.c dllplug.c \
	dllnext.c dlllook.c dllscan.c -ldl -ldld

$(INCLUDEDIR) :INSTALLPROTO: dlldefs.h

dlldefs.h :COPY: FEATURE/dll

:: RELEASE
