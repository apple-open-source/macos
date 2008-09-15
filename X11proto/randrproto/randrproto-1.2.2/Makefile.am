randrdir = $(includedir)/X11/extensions
randr_HEADERS = \
	randr.h \
	randrproto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = randrproto.pc

randrdocdir = $(datadir)/doc/$(PACKAGE)
randrdoc_DATA = randrproto.txt

EXTRA_DIST = autogen.sh randrproto.pc.in $(randrdoc_DATA)

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git-log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
