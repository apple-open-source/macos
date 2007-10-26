SUBDIRS = include src man

aclocaldir = $(datadir)/aclocal
aclocal_DATA = xaw.m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 

if BUILD_XAW6

pkgconfig_DATA += xaw6.pc
DEFAULT_LIB = libXaw.6.dylib

endif

if BUILD_XAW7

pkgconfig_DATA += xaw7.pc
DEFAULT_LIB = libXaw.7.dylib

endif

if BUILD_XAW8

pkgconfig_DATA += xaw8.pc
DEFAULT_LIB = libXaw.8.dylib

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
	cd $(DESTDIR)$(libdir) && rm -f libXaw.dylib && ln -s $(DEFAULT_LIB) libXaw.dylib
	cd $(DESTDIR)$(libdir) && rm -f libXaw6.so.6 libXaw7.so.7 libXaw8.so.8
	cd $(DESTDIR)$(libdir) && rm -f libXaw6.dylib.6 libXaw7.dylib.7 libXaw8.dylib.8
	cd $(DESTDIR)$(libdir) && ln -s libXaw6.dylib libXaw.6.dylib
	cd $(DESTDIR)$(libdir) && ln -s libXaw7.dylib libXaw.7.dylib
	cd $(DESTDIR)$(libdir) && ln -s libXaw8.dylib libXaw.8.dylib
endif
