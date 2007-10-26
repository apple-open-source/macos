lib_LTLIBRARIES = libXv.la

libXv_la_SOURCES = \
         Xv.c \
         Xvlibint.h

libXv_la_LIBADD = @XV_LIBS@
AM_CFLAGS = @XV_CFLAGS@ -I$(top_srcdir)/include @MALLOC_ZERO_CFLAGS@

libXv_la_LDFLAGS = -version-number 1:0:0 -no-undefined

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(libXv_la_SOURCES) $(XV_LIBS)
endif LINT

if MAKE_LINT_LIB
lintlibdir = $(libdir)

lintlib_DATA = $(LINTLIB)

$(LINTLIB): $(libXau_la_SOURCES)
	$(LINT) -y -oXv -x $(ALL_LINT_FLAGS) $(libXv_la_SOURCES) 
endif MAKE_LINT_LIB

