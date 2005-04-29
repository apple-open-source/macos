##
# Makefile for GNU m4
##

# Project info
Project		    = m4
ProjectName	    = gm4
UserType	    = Developer
ToolType	    = Commands
GnuAfterInstall	= after_install install-plist install-info
Extra_CC_Flags  = -mdynamic-no-pic

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

# Install gm4 for compatibilty with previous releases.
after_install:
	$(STRIP) $(DSTROOT)/usr/bin/m4
	$(LN) $(DSTROOT)/usr/bin/m4 $(DSTROOT)/usr/bin/gm4

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(OSV)/$(ProjectName).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(ProjectName).txt

install-info:
	$(INSTALL_DIRECTORY) "$(DSTROOT)/Developer/ADC Reference Library/documentation/DeveloperTools"
	$(TEXI2HTML) --split=chapter $(Sources)/doc/m4.texinfo \
		--output="$(DSTROOT)/Developer/ADC Reference Library/documentation/DeveloperTools/m4"

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.4.2
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-doc__Makefile.in

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif
