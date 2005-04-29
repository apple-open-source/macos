#
# xbs-compatible wrapper Makefile for ncurses.
#

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
APPLE_INTERNAL_DIR=/AppleInternal
RC_ARCHS=

ORDERFILE = $(APPLE_INTERNAL_DIR)/OrderFiles/libncurses.order

ifeq ($(shell test -f "$(SECTORDER_FILE)" && echo "YES"), "YES")
	SECTORDER_FLAGS = -Wl,-sectorder,__TEXT,__text,$(ORDERFILE)
endif

ENV=	APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
	CFLAGS="$(RC_ARCHS:%=-arch %) -O" \
	RC_ARCHS="$(RC_ARCHS)" \
	DYLD_LIBRARY_PATH="$(DSTROOT)/usr/lib"

INSTALLED_BINS := clear infocmp tack tic toe tput tset
INSTALLED_STLIBS := libcurses.a libform.a libmenu.a libncurses.a libpanel.a
INSTALLED_DYLIBS := libform.5.dylib libmenu.5.dylib libncurses.5.dylib libpanel.5.dylib

installsrc :
	if test ! -d $(SRCROOT)/ncurses ; then mkdir -p $(SRCROOT)/ncurses; fi;
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	for i in `find $(SRCROOT) | grep "CVS$$"` ; do \
		if test -d $$i ; then \
			rm -rf $$i; \
		fi; \
	done

installhdrs:
	true

old_installhdrs :
	$(SHELL) -ec \
	'cd $(SRCROOT)/ncurses; \
	$(ENV) ./configure --prefix=/usr --with-shared --without-debug --enable-termcap --without-cxx-binding --without-cxx; \
	$(ENV) $(MAKE) DESTDIR=$(DSTROOT) install.includes; \
	$(ENV) $(MAKE) distclean'

install :
	$(SHELL) -ec \
	'cd $(SRCROOT)/ncurses; \
	$(ENV) ./configure --prefix=/usr --with-shared --without-debug --enable-termcap --without-cxx-binding --without-cxx; \
	$(ENV) $(MAKE); \
	$(ENV) $(MAKE) DESTDIR=$(DSTROOT) install; \
	$(ENV) $(MAKE) distclean; \
	rm $(DSTROOT)/usr/lib/lib*.a; \
	rm $(DSTROOT)/usr/lib/terminfo; \
	mkdir -p $(DSTROOT)/usr/share; \
	rm -rf $(DSTROOT)/usr/bin $(DSTROOT)/usr/include $(DSTROOT)/usr/man $(DSTROOT)/usr/share; \
	for l in $(INSTALLED_DYLIBS) ; do \
		strip -x $(DSTROOT)/usr/lib/$${l}; \
	done; \
	rm $(DSTROOT)/usr/lib/*[a-z].dylib'

clean :
