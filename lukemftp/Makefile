#
# xbs-compatible Makefile for lukemftp.
#

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=

PROJNAME=lukemftp

ENV=	CFLAGS="$(RC_ARCHS:%=-arch %) -no-cpp-precomp -O"

.PHONY : installsrc installhdrs install clean

installsrc :
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	for i in `find $(SRCROOT) | grep "CVS$$"` ; do \
		if test -d $$i ; then \
			rm -rf $$i; \
		fi; \
	done

installhdrs :

install :
	$(SHELL) -ec \
	'mkdir -p $(OBJROOT)/$(PROJNAME); \
	cd $(OBJROOT)/$(PROJNAME); \
	$(ENV) $(SRCROOT)/$(PROJNAME)/configure --prefix=/usr --enable-ipv6 ; \
	$(MAKE); \
	$(MAKE) bindir=$(DSTROOT)/usr/bin mandir=$(DSTROOT)/usr/share/man install; \
	strip -x $(DSTROOT)/usr/bin/ftp'

clean:
