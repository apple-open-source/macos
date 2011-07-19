SUBDIRS=specs

xcmiscdir = $(includedir)/X11/extensions
xcmisc_HEADERS = \
	xcmiscproto.h \
	xcmiscstr.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xcmiscproto.pc


MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
