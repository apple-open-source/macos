lib_LTLIBRARIES = libXvMC.la libXvMCW.la

libXvMC_la_SOURCES = \
         XvMC.c \
         XvMClibint.h

libXvMCW_la_SOURCES = \
	XvMCWrapper.c

AM_CFLAGS = @XVMC_CFLAGS@ -I$(top_srcdir)/include @MALLOC_ZERO_CFLAGS@

libXvMC_la_LIBADD = @XVMC_LIBS@
libXvMC_la_CFLAGS = $(AM_CFLAGS)

libXvMCW_la_LIBADD = @XVMC_LIBS@
libXvMCW_la_CFLAGS =				\
	$(AM_CFLAGS)				\
	-DXVMC_CONFIGDIR=$(sysconfdir)/X11		\
	-DXVMC_SOVERSION=\".1.0\"		\
	-DXV_SOVERSION=\".1.0\"

libXvMC_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libXvMCW_la_LDFLAGS = -version-number 1:0:0 -no-undefined


if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
                $(AM_CPPFLAGS) $(CPPFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(libXvMC_la_SOURCES) $(XVMC_LIBS)
	$(LINT) $(ALL_LINT_FLAGS) $(libXvMCW_la_CFLAGS) $(libXvMCW_la_SOURCES) $(XVMC_LIBS)
endif LINT
