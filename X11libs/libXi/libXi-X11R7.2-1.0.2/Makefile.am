# $XdotOrg: $

AM_CFLAGS = $(XI_CFLAGS)

SUBDIRS = src man

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xi.pc

EXTRA_DIST = xi.pc.in autogen.sh ChangeLog

CLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	GIT_DIR=${srcdir}/.git git-log > ChangeLog

dist-hook: ChangeLog
