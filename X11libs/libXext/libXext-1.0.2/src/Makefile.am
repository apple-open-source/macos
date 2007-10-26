lib_LTLIBRARIES=libXext.la

AM_CFLAGS=$(XEXT_CFLAGS) $(MALLOC_ZERO_CFLAGS)

libXext_la_LDFLAGS = -version-number $(XEXT_SOREV) -no-undefined

libXext_la_LIBADD = $(XEXT_LIBS)

libXext_la_SOURCES = \
	DPMS.c \
	MITMisc.c \
	XAppgroup.c \
	XEVI.c \
	XLbx.c \
	XMultibuf.c \
	XSecurity.c \
	XShape.c \
	XShm.c \
	XSync.c \
	XTestExt1.c \
	Xcup.c \
	Xdbe.c \
	extutil.c \
	globals.c

