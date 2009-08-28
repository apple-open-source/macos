inputdir = $(includedir)/X11/extensions
input_HEADERS = \
	XI.h \
	XInput.h \
	XIproto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = inputproto.pc

EXTRA_DIST = autogen.sh inputproto.pc.in

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git-log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
