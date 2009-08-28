randrdir = $(includedir)/X11/extensions
randr_HEADERS = \
	randr.h \
	randrproto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = randrproto.pc

randrdocdir = $(datadir)/doc/$(PACKAGE)
randrdoc_DATA = randrproto.txt

EXTRA_DIST = randrproto.pc.in $(randrdoc_DATA)

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog
