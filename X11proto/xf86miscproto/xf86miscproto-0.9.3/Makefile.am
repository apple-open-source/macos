xf86miscdir = $(includedir)/X11/extensions
xf86misc_HEADERS = \
	xf86misc.h \
	xf86mscstr.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xf86miscproto.pc

EXTRA_DIST = xf86miscproto.pc.in

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog
