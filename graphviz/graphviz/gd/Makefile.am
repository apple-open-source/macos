## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES = ${FT_INCLUDES} \
	${TTF_INCLUDES} \
	${PNG_INCLUDES} \
	${JPEG_INCLUDES} \
	${Z_INCLUDES}

# avoid X dependencies
#	${XPM_INCLUDES}

LIBS = ${FT_LIBS} \
	${PNG_LIBS} \
	${JPEG_LIBS} \
	${Z_LIBS} \
	${ICONV_LIBS} \
	-lm

# avoid X dependencies
#	${XPM_LIBS}

pkginclude_HEADERS = gd.h gd_io.h gdcache.h gdfontg.h gdfontl.h gdfontmb.h \
	gdfonts.h gdfontt.h gdhelpers.h jisx0208.h wbmp.h
# pkglib_LTLIBRARIES = libgd.la
noinst_LTLIBRARIES = libgdconvenience.la
noinst_PROGRAMS = fontwheeltest fontsizetest gdtestft gddemo

libgd_la_SOURCES = gd.c gd_gd.c gd_gd2.c gd_io.c gd_io_dp.c \
	gd_io_file.c gd_ss.c gd_io_ss.c gd_gif.c gd_jpeg.c gd_png.c \
	gdcache.c gdfontg.c gdfontl.c gdfontmb.c gdfonts.c gdfontt.c \
	gdhelpers.c gdkanji.c gdtables.c gdft.c gdxpm.c \
	wbmp.c gd_wbmp.c gd_topal.c ${pkginclude_HEADERS}

libgdconvenience_la_SOURCES = ${libgd_la_SOURCES}

fontwheeltest_SOURCES = fontwheeltest.c
fontwheeltest_LDADD = libgdconvenience.la

fontsizetest_SOURCES = fontsizetest.c
fontsizetest_LDADD = libgdconvenience.la

gdtestft_SOURCES = gdtestft.c
gdtestft_LDADD = libgdconvenience.la

gddemo_SOURCES = gddemo.c
gddemo_LDADD = libgdconvenience.la

# keep the contents of the original gd-2.0.15 distribution
EXTRA_DIST = bdftogd \
	gd2copypal.c gd2time.c gd2topng.c gddemo.c testac.c \
	gdparttopng.c gdtest.c gdtopng.c giftogd.c mathmake.c \
	pngtogd.c pngtogd2.c webpng.c annotate.c \
	install-item index.html readme.jpn README-JPEG.TXT \
	COPYING INSTALL README.TXT \
	aclocal.m4 config.hin configure.ac configure.pl \
	install-sh makefile.sample missing mkinstalldirs \
	demoin.png Makefile.old \
	Makefile.am.orig Makefile.in.orig \
	config test
