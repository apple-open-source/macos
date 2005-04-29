##
# Makefile for make
##

# Project info
Project               = make
ProjectName           = gnumake
UserType              = Developer
ToolType              = Commands
Extra_Configure_Flags = --program-prefix="gnu" --disable-nls
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = install-html install-links install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-html:
	$(MAKE) -C $(BuildDirectory) html
	$(INSTALL_DIRECTORY) $(RC_Install_HTML)
	$(INSTALL_FILE) $(BuildDirectory)/doc/make/*.html $(RC_Install_HTML)

install-links:
	@echo "Installing make symlink"
	$(LN) -fs gnumake $(DSTROOT)$(USRBINDIR)/make
	$(LN) -fs gnumake.1 $(DSTROOT)/usr/share/man/man1/make.1
	$(RM) $(DSTROOT)/usr/share/info/dir

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 3.80
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-Makefile.in \
                 patch-default.c \
                 patch-expand.c \
                 patch-file.c \
                 patch-filedef.h \
                 patch-implicit.c \
                 patch-job.c \
                 patch-main.c \
                 patch-make.h \
                 patch-next.c \
                 patch-read.c \
                 patch-remake.c \
                 patch-variable.c \
                 patch-variable.h

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

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(OSV)/$(ProjectName).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(ProjectName).txt
