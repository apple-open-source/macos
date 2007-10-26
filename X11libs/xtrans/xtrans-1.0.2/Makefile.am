Xtransincludedir = $(includedir)/X11/Xtrans
Xtransinclude_HEADERS = \
	Xtrans.h \
	Xtrans.c \
	Xtransdnet.c \
	Xtransint.h \
	Xtranslcl.c \
	Xtransos2.c \
	Xtranssock.c \
	Xtranstli.c \
	Xtransutil.c \
	transport.c

aclocaldir = $(datadir)/aclocal
aclocal_DATA = xtrans.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xtrans.pc

EXTRA_DIST = xtrans.pc.in ${aclocal_DATA}

CLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	git-log > ChangeLog

dist-hook: ChangeLog
