applewmdir = $(includedir)/X11/extensions
applewm_HEADERS = \
        applewm.h \
	applewmstr.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = applewmproto.pc

EXTRA_DIST = applewmproto.pc.in

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog
