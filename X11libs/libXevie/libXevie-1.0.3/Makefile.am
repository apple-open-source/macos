SUBDIRS = src man

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xevie.pc

MAINTAINERCLEANFILES = ChangeLog INSTALL
EXTRA_DIST = xevie.pc.in 

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

if LINT
# Check source code with tools like lint & sparse
lint:
	cd src && $(MAKE) $(AM_MAKEFLAGS) lint
endif LINT
