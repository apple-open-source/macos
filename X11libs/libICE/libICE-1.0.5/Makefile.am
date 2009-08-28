SUBDIRS = src

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ice.pc

EXTRA_DIST=ice.pc.in

.PHONY: ChangeLog

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog

if LINT
lint:
	(cd src && $(MAKE) $(MFLAGS) lint)
endif LINT
