#
# xbs-compatible Makefile for lukemftpd.
#

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=

PROJNAME=lukemftpd

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
	$(ENV) $(SRCROOT)/$(PROJNAME)/configure --prefix=/usr --enable-ipv6; \
	$(MAKE); \
	$(MAKE) sbindir=$(DSTROOT)/usr/libexec mandir=$(DSTROOT)/usr/share/man install; \
	strip -x $(DSTROOT)/usr/libexec/ftpd'

clean:
