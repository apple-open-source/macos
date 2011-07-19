dmxdir = $(includedir)/X11/extensions
dmx_HEADERS = \
	dmx.h \
	dmxproto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dmxproto.pc


MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
