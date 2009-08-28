##
# Makefile for autoconf
##

# Project info
Project         = autoconf
UserType        = Developer
ToolType        = Commands
GnuAfterInstall = remove-dir install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

remove-dir:
	rm $(DSTROOT)/usr/share/info/dir

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 2.61
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-Makefile.in patch-doc__Makefile.in patch-3505843-glibtoolize.in patch-5828374-lib_autoconf_libs.m4.in patch-5609273-Xcode.in patch-6073314-rm-dSYM.in

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xof $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(AEP_Patches); do \
		patch -p0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
endif
