xineramadir = $(includedir)/X11/extensions
xinerama_HEADERS = \
	panoramiXproto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xineramaproto.pc


MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
