##
# Makefile for cscope
##

# Project info
Project               = cscope
UserType              = Developer
ToolType              = Commands
Extra_Environment     =   AUTOCONF="$(Sources)/missing autoconf"        \
                        AUTOHEADER="$(Sources)/missing autoheader"
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = install-plist post-install 

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP	       = YES
AEP_Project    = $(Project)
AEP_Version    = 15.6
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = ncurses.patch sprintf.patch

ifeq ($(suffix $(AEP_Filename)),.bz2)
    AEP_ExtractOption = j
else
    AEP_ExtractOption = z
endif

install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
	    (cd $(SRCROOT)/$(Project) && patch -p0 -F0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

post-install:
	$(INSTALL_FILE) $(SRCROOT)/patches/ocs.1 $(DSTROOT)/usr/share/man/man1/
	$(MKDIR) -p $(DSTROOT)/usr/local
	$(MV) $(DSTROOT)/usr/bin $(DSTROOT)/usr/share $(DSTROOT)/usr/local
