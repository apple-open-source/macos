##
# Makefile for uucp
## 

# Project info
Project           = uucp
UserType          = Developer
ToolType          = Commands
GnuNoChown=YES
GnuAfterInstall     = post-install install-plist
Extra_Configure_Flags= --with-newconfigdir=/private/etc/uucp --with-user=_uucp

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Directories that will be created and owned by _uucp
UUCP_DIRS=$(DSTROOT)/private/var/log/uucp $(DSTROOT)/private/var/spool/uucp

post-install:
	$(MKDIR) $(UUCP_DIRS) $(DSTROOT)/private/etc/uucp
	$(INSTALL_FILE) $(SRCROOT)/configs/passwd  $(DSTROOT)/private/etc/uucp
	$(INSTALL_FILE) $(SRCROOT)/configs/port    $(DSTROOT)/private/etc/uucp
	$(INSTALL_FILE) $(SRCROOT)/configs/sys     $(DSTROOT)/private/etc/uucp
	$(CHOWN) _uucp $(UUCP_DIRS)

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL_FILE) $(SRCROOT)/com.apple.uucp.plist $(DSTROOT)/System/Library/LaunchDaemons
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/uucp.plist $(OSV)/uucp.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/uucp.txt
	# there isn't a configuure option to not setuid a bunch of stuff, so we let it do that and then we "fix it"
	chmod -R ug-s $(DSTROOT)/usr/*bin


# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.07
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = PR-3996371-conf.diff \
	         PR-4883535.patch \
		 PR-4383204.patch \
		 PR-4905054.manpages.patch \
		 rdar-25537031-fix-uuconv.c.diff \
		 PR-59291737.patch

AEP_SubDir     = uucp
AEP_TarDir     := $(shell pwd)

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif


# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(AEP_TarDir)/$(AEP_Filename)
	-$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	ls $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
	    cd $(SRCROOT)/$(Project) && patch -p0 < $(AEP_TarDir)/patches/$$patchfile; \
	done
	for newfile in $(shell find $(AEP_TarDir)/new -maxdepth 1 -type f); do \
	    echo copying $$newfile; \
	    cp -v $$newfile $(SRCROOT)/$(Project)/; \
	done
endif

install::
	chgrp -R wheel $(DSTROOT)
	rm $(DSTROOT)/usr/share/info/dir
