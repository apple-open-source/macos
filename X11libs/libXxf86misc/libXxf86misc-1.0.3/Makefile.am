SUBDIRS = src man

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xxf86misc.pc

MAINTAINERCLEANFILES = ChangeLog INSTALL
EXTRA_DIST = xxf86misc.pc.in 

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
