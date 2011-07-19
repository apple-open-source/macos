lib_LTLIBRARIES = libXevie.la

libXevie_la_SOURCES = Xevie.c

libXevie_la_LIBADD = $(XEVIE_LIBS)

AM_CFLAGS = $(CWARNFLAGS) $(XEVIE_CFLAGS)

INCLUDES = -I$(top_srcdir)/include

libXevie_la_LDFLAGS = -version-number 1:0:0 -no-undefined

libXevieincludedir = $(includedir)/X11/extensions
libXevieinclude_HEADERS = $(top_srcdir)/include/X11/extensions/Xevie.h 

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
                $(AM_CPPFLAGS) $(CPPFLAGS) $(XEVIE_CFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(libXevie_la_SOURCES) $(XEVIE_LIBS)
endif LINT

if MAKE_LINT_LIB
lintlibdir = $(libdir)

lintlib_DATA = $(LINTLIB)

$(LINTLIB): $(libXevie_la_SOURCES)
	$(AM_V_GEN)$(LINT) -y -oXevie -x $(ALL_LINT_FLAGS) $(libXevie_la_SOURCES)
endif MAKE_LINT_LIB

CLEANFILES = $(lintlib_DATA)
