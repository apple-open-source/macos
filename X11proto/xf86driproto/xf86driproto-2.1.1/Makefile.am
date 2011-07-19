xf86dridir = $(includedir)/X11/dri
xf86dri_HEADERS = xf86dri.h xf86dristr.h xf86driproto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xf86driproto.pc


MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
