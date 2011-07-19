xf86bigfontdir = $(includedir)/X11/extensions
xf86bigfont_HEADERS = \
	xf86bigfont.h \
	xf86bigfproto.h \
	xf86bigfstr.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xf86bigfontproto.pc

EXTRA_DIST = xf86bigfontproto.pc.in

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog
