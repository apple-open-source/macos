#
# xbs-compatible Makefile for lukemftpd.
#

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
TAR ?= /usr/bin/gnutar
MV ?= /bin/mv

SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
ETCDIR=/private/etc
RC_ARCHS=

Project=tnftpd

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 20040810
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = PR-2571387-pw.db.patch PR-3285536.pamify.patch PR-3795936.long-username.patch \
                 PR-3886477.Makefile.in.patch PR-3886477.ftpd.c.patch NLS_PR-3955757.patch

ENV=	CFLAGS="$(RC_ARCHS:%=-arch %) -no-cpp-precomp -Os -mdynamic-no-pic"

.PHONY : copysrc installsrc installhdrs install clean

installhdrs :

copysrc :
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	for i in `find $(SRCROOT) | grep "CVS$$"` ; do \
		if test -d $$i ; then \
			rm -rf $$i; \
		fi; \
	done

install :
	$(SHELL) -ec \
	'mkdir -p $(OBJROOT)/$(Project); \
	cd $(OBJROOT)/$(Project); \
	$(ENV) $(SRCROOT)/$(Project)/configure --prefix=/usr --sysconfdir="$(ETCDIR)" --enable-ipv6; \
	$(MAKE); \
	$(MAKE) sbindir=$(DSTROOT)/usr/libexec mandir=$(DSTROOT)/usr/share/man install; \
	strip -x $(DSTROOT)/usr/libexec/tnftpd'
	mv $(DSTROOT)/usr/libexec/tnftpd $(DSTROOT)/usr/libexec/ftpd
	mkdir -p $(DSTROOT)/private/etc/pam.d/
	cp ftpd $(DSTROOT)/private/etc/pam.d/
	mkdir -p $(DSTROOT)/System/Library/LaunchDaemons
	install -c -m 0644 $(SRCROOT)/ftp.plist $(DSTROOT)/System/Library/LaunchDaemons

clean:
#
# xbs-compatible Makefile for lukemftpd.
#

install-pam-item :
	strip -x $(DSTROOT)/usr/libexec/ftpd'
	mkdir -p $(DSTROOT)/private/etc/pam.d/
	cp ftpd $(DSTROOT)/private/etc/pam.d/

# It's a GNU Source project
#include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make


ifeq ($(suffix $(AEP_Filename)),.bz2)
    AEP_ExtractOption = j
else
    AEP_ExtractOption = z
endif

installsrc:: copysrc
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
	    cd $(SRCROOT)/$(Project) && patch -lp0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif

SV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

#install-plist:
#	$(MKDIR) $(OSV)
#	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
#	$(MKDIR) $(OSL)
#	$(INSTALL_FILE) $(Sources)/LICENCE $(OSL)/$(Project).txt
