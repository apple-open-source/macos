SUBDIRS = include src man

aclocaldir = $(datadir)/aclocal
aclocal_DATA = xaw.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 

if BUILD_XAW6

pkgconfig_DATA += xaw6.pc
DEFAULT_LIB = libXaw6.so

endif

if BUILD_XAW7

pkgconfig_DATA += xaw7.pc
DEFAULT_LIB = libXaw7.so

endif

if BUILD_XAW8

pkgconfig_DATA += xaw8.pc
DEFAULT_LIB = libXaw8.so

endif

EXTRA_DIST = xaw6.pc.in \
	xaw7.pc.in \
	xaw8.pc.in \
	autogen.sh \
	xaw.m4 \
	old-doc/Changelog \
	old-doc/CHANGES

if !PLATFORM_WIN32
install-exec-hook:
	cd $(DESTDIR)$(libdir) && rm -f libXaw.so && ln -s $(DEFAULT_LIB) libXaw.so
endif
