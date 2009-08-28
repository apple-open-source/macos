##
# Makefile for rsync
##

# Project info
Project         = rsync
UserType        = Administration
ToolType        = Commands
GnuAfterInstall = install-plist populate-symroot
CommonNoInstallSource	= YES

ifeq ($(shell tconf --test TARGET_OS_EMBEDDED),YES)
GnuAfterInstall+= install-config
endif

ifneq ($(MACOSX_DEPLOYMENT_TARGET),10.4)
NO_POINTER_SIGN=-Wno-pointer-sign
endif
SDKROOT ?= /
# CFLAGS is set in the Makefile, but overridden in the environment.
# To work around, just pass the extra flags that the Makefile contains.
Extra_CC_Flags  = -mdynamic-no-pic -DHAVE_CONFIG_H -I$(Sources)/popt -D_FORTIFY_SOURCE=2 \
	$(NO_POINTER_SIGN) -Wno-discard-qual \
	$(ifneq /,$(SDKROOT),-isysroot $(SDKROOT))
Extra_Configure_Flags = --enable-ea-support ac_cv_sizeof_long="__WORDSIZE/8"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = NO
AEP_Project    = $(Project)
AEP_Version    = 2.6.9
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    =

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
else
	rsync -aC ./ "$(SRCROOT)/"
endif

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

install-config:
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/etc
	$(INSTALL_FILE) $(SRCROOT)/rsyncd.conf $(DSTROOT)/private/etc
	$(INSTALL_DIRECTORY) $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL_FILE) $(SRCROOT)/launchd-rsync.plist $(DSTROOT)/System/Library/LaunchDaemons/rsync.plist

populate-symroot:
	$(CP) $(OBJROOT)/rsync $(SYMROOT)
