#
# xbs-compatible Makefile for bzip2.
#

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=

PROJNAME=bzip2

ENV=	CFLAGS="$(RC_ARCHS:%=-arch %) -no-cpp-precomp -O -D_FILE_OFFSET_BITS=64"

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
	'cd $(SRCROOT)/$(PROJNAME); \
	$(MAKE) $(ENV); \
	$(MAKE) $(ENV) PREFIX=$(DSTROOT)/usr install; \
	$(MAKE) distclean; \
	strip -x $(DSTROOT)/usr/bin/bzip2; \
	strip -x $(DSTROOT)/usr/bin/bunzip2; \
	strip -x $(DSTROOT)/usr/bin/bzcat; \
	strip -x $(DSTROOT)/usr/bin/bzip2recover; \
	install -d $(DSTROOT)/usr/share; \
	mv $(DSTROOT)/usr/man $(DSTROOT)/usr/share; \
	ln $(DSTROOT)/usr/share/man/man1/bzip2.1 $(DSTROOT)/usr/share/man/man1/bunzip2.1; \
	ln $(DSTROOT)/usr/share/man/man1/bzip2.1 $(DSTROOT)/usr/share/man/man1/bzcat.1; \
	ln $(DSTROOT)/usr/share/man/man1/bzip2.1 $(DSTROOT)/usr/share/man/man1/bzip2recover.1'
	ranlib $(DSTROOT)/usr/lib/libbz2.a

clean:
