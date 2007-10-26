#
# xbs-compatible Makefile for gnuserv.
#

SHELL := /bin/sh

Project = gnuserv

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=

ENV=	CFLAGS="$(RC_ARCHS:%=-arch %) -no-cpp-precomp -mdynamic-no-pic -Os"

MKDIR = mkdir -p -m 0755
INSTALL_FILE = install -o root -g wheel -m 0644

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

# Since the vendor (Apple) is installing this, it must not go in the site-lisp
# directory.  Unfortunately, that means hard coding  the path to the emacs lisp
# directory, which happens to have the emacs version number embedded in it.  As
# such, this variable needs updated every time the emacs version changes.
EMACS_VERSION := $(shell /usr/libexec/dumpemacs -V)
# byte-compile using temacs (3325521)
TEMACS=/usr/bin/emacs-undumped

INSTALLED_BINS := gnuserv gnuclient

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
	'cd $(SRCROOT)/gnuserv; \
	$(ENV) ./configure --prefix=$(DSTROOT)/usr; \
	$(ENV) $(MAKE) EMACS="$(TEMACS)"; \
	$(ENV) $(MAKE) install; \
	$(ENV) $(MAKE) distclean; \
	mkdir -p $(DSTROOT)/usr/share; \
	mv $(DSTROOT)/usr/man $(DSTROOT)/usr/share/man; \
	mkdir -p $(DSTROOT)/usr/share/emacs/$(EMACS_VERSION); \
	mv $(DSTROOT)/usr/share/emacs/site-lisp \
	   $(DSTROOT)/usr/share/emacs/$(EMACS_VERSION)/lisp; \
	for b in $(INSTALLED_BINS) ; do \
		strip -x $(DSTROOT)/usr/bin/$${b}; \
	done'
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/COPYING $(OSL)/$(Project).txt

clean:
