#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
sbin_PROGRAMS = ntpdate ntptimeset
ntpdate_SOURCES = ntpdate.c ntptime_config.c
ntptimeset_SOURCES = ntptimeset.c ntptime_config.c
INCLUDES = -I$(top_srcdir)/include
# LDADD might need RESLIB and ADJLIB
LDADD =	version.o ../libntp/libntp.a @LIBRSAREF@
DISTCLEANFILES = .version version.c stamp-v
noinst_HEADERS = ntpdate.h
#EXTRA_DIST = ntpdate.mak
ETAGS_ARGS = Makefile.am

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

version.o: $(ntpdate_OBJECTS) ../libntp/libntp.a @LIBRSAREF@ Makefile
	env CSET=`cat $(top_srcdir)/version` $(top_builddir)/scripts/mkver ntpdate
	$(COMPILE) -c version.c
