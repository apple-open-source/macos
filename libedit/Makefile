#
# xbs-compatible Makefile for libedit.
#

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=

ENV=	CFLAGS="$(RC_ARCHS:%=-arch %)" \
	S_LDFLAGS="$(RC_ARCHS:%=-arch %)"

INSTALLED_STLIBS := libedit.a
INSTALLED_DYLIBS := libedit.2.dylib

.PHONY : installsrc installhdrs install clean

installsrc :
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	for i in `find $(SRCROOT) | grep "CVS$$"` ; do \
		if test -d $$i ; then \
			rm -rf $$i; \
		fi; \
	done

installhdrs :
	$(SHELL) -ec \
	'cd $(SRCROOT)/libedit; \
	$(ENV) ./configure --prefix=/usr --disable-readline; \
	$(ENV) $(MAKE) PREFIX=$(DSTROOT)/usr install_hdr'

install :
	$(SHELL) -ec \
	'cd $(SRCROOT)/libedit; \
	$(ENV) ./configure --prefix=/usr --disable-readline; \
	$(ENV) $(MAKE); \
	$(ENV) $(MAKE) PREFIX=$(DSTROOT)/usr install; \
	$(ENV) $(MAKE) distclean; \
	mkdir -p $(DSTROOT)/usr/share; \
	mv $(DSTROOT)/usr/man $(DSTROOT)/usr/share/man; \
	mkdir -p $(DSTROOT)/usr/local/lib; \
	for l in $(INSTALLED_STLIBS) ; do \
		mv $(DSTROOT)/usr/lib/$${l} $(DSTROOT)/usr/local/lib/; \
	done; \
	for l in $(INSTALLED_DYLIBS) ; do \
		strip -x $(DSTROOT)/usr/lib/$${l}; \
	done'

clean:
