xf86dridir = $(includedir)/X11/dri
xf86dri_HEADERS = xf86dri.h xf86dristr.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xf86driproto.pc

EXTRA_DIST = autogen.sh xf86driproto.pc.in

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git-log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
