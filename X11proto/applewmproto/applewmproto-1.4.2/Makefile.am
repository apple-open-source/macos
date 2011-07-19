applewmdir = $(includedir)/X11/extensions
applewm_HEADERS = \
        applewmconst.h \
	applewmproto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = applewmproto.pc


MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

ChangeLog:
	$(CHANGELOG_CMD)
INSTALL:
	$(INSTALL_CMD)

dist-hook: ChangeLog INSTALL
