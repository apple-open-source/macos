SUBDIRS=specs

fontsdir = $(includedir)/X11/fonts
fonts_HEADERS = \
	font.h \
	fontproto.h \
	fontstruct.h \
	FS.h \
	fsmasks.h \
	FSproto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = fontsproto.pc


MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
