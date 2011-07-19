dri2dir = $(includedir)/X11/extensions
dri2_HEADERS = dri2proto.h dri2tokens.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dri2proto.pc

EXTRA_DIST = dri2proto.pc.in dri2proto.txt

MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
