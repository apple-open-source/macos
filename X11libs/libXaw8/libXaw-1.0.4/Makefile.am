SUBDIRS = include src man

aclocaldir = $(datadir)/aclocal
aclocal_DATA = xaw.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 

if BUILD_XAW6

pkgconfig_DATA += xaw6.pc
if PLATFORM_DARWIN
DEFAULT_LIB = libXaw.6.@LIBEXT@
else
DEFAULT_LIB = libXaw6.@LIBEXT@
endif

endif

if BUILD_XAW7

pkgconfig_DATA += xaw7.pc
if PLATFORM_DARWIN
DEFAULT_LIB = libXaw.7.@LIBEXT@
else
DEFAULT_LIB = libXaw7.@LIBEXT@
endif

endif

if BUILD_XAW8

pkgconfig_DATA += xaw8.pc
if PLATFORM_DARWIN
DEFAULT_LIB = libXaw.8.@LIBEXT@
else
DEFAULT_LIB = libXaw8.@LIBEXT@
endif

endif

EXTRA_DIST = xaw6.pc.in \
	xaw7.pc.in \
	xaw8.pc.in \
	autogen.sh \
	xaw.m4 \
	old-doc/Changelog \
	old-doc/CHANGES \
	ChangeLog

if !PLATFORM_WIN32
install-exec-hook:
	cd $(DESTDIR)$(libdir) && rm -f libXaw.@LIBEXT@ && ln -s $(DEFAULT_LIB) libXaw.@LIBEXT@
endif

MAINTAINERCLEANFILES=ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git-log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
