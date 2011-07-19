##
# Makefile for lukemftp.
##

# Project Info
Project         = tnftp
Extra_CC_Flags  = -Wall -mdynamic-no-pic -O2
GnuNoBuild          = YES
GnuAfterInstall     = post-install install-plist
Extra_Configure_Flags     += --prefix=/usr --enable-ipv6 --bindir=$(DSTROOT)/usr/bin --mandir=$(DSTROOT)/usr/share/man

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target      = install
Install_Flags       = DESTDIR=$(DSTROOT)

build:: configure
	$(_v) $(MAKE) -C $(BuildDirectory)


post-install:
	$(STRIP) -x $(DSTROOT)/usr/bin/ftp

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/lukemftp.plist $(OSV)/lukemftp.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/lukemftp.txt

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 20070806
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = \
		Makefile.in.patch \
		PR-4074918.ftp.c.patch \
		configure.patch \
		PR-7577277.cmds.c.patch \
		PR-7577277.extern.h.patch \
		PR-7577277.fetch.c.patch \
		PR-7577277.main.c.patch \
		PR-7577277.util.c.patch

install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
            (cd $(SRCROOT)/$(Project) && patch -lp0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif



