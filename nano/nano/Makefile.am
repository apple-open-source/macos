## $Id: Makefile.am,v 1.30 2003/02/13 12:50:53 jordi Exp $
AUTOMAKE_OPTIONS = gnu no-dependencies

DEFS= -DSYSCONFDIR=\"$(sysconfdir)\"

bin_PROGRAMS = 	nano
nano_SOURCES =	color.c \
		cut.c \
		files.c \
		global.c \
		move.c \
		nano.c \
		nano.h \
		proto.h \
		rcfile.c \
		search.c \
		utils.c \
		winio.c 

man_MANS = nano.1 nanorc.5
nano_LDADD = @GLIB_LIBS@ @LIBINTL@

info_TEXINFOS = nano.texi
MAKEINFO = makeinfo --no-split

EXTRA_DIST =	ABOUT-NLS AUTHORS BUGS COPYING ChangeLog INSTALL NEWS \
		README THANKS TODO UPGRADE config.rpath install-sh missing \
		mkinstalldirs nano.1.html nanorc.5.html faq.html \
		nanorc.sample nano.spec $(man_MANS)

SUBDIRS = po m4

localedir = $(datadir)/locale
INCLUDES = -Iintl -DLOCALEDIR=\"$(localedir)\" -I@includedir@

ACLOCAL_AMFLAGS = -I m4
