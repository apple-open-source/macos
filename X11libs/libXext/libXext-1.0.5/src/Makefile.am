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
        Xge.c \
	extutil.c \
	globals.c

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
                $(AM_CPPFLAGS) $(CPPFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(libXext_la_SOURCES) $(XEXT_LIBS)
endif LINT

if MAKE_LINT_LIB
lintlibdir = $(libdir)

lintlib_DATA = $(LINTLIB)

$(LINTLIB): $(libXext_la_SOURCES)
	$(LINT) -y -oXext -x $(ALL_LINT_FLAGS) $(libXext_la_SOURCES) 
endif MAKE_LINT_LIB
