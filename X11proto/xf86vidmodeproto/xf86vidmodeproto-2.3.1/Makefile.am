xf86vidmodedir = $(includedir)/X11/extensions
xf86vidmode_HEADERS = \
	xf86vm.h \
	xf86vmproto.h \
	xf86vmstr.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xf86vidmodeproto.pc


MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
