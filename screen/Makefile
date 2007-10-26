##
# Makefile for screen
##

# Project info
Project               = screen
UserType              = Administrator
ToolType              = Commands
GnuAfterInstall       = install-strip install-plist
Extra_Configure_Flags = --with-sys-screenrc=$(ETCDIR)/screenrc
Extra_CC_Flags        = -mdynamic-no-pic -DRUN_LOGIN
Extra_Install_Flags   = DSTROOT="$(DSTROOT)"

# 5280670
Extra_CC_Flags += -fno-altivec

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target        = install

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 4.0.3
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = Makefile.in.diff config.h.in.diff configure.diff pty.c.diff window.c.diff screen.c.diff

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
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif

install-strip:
	$(STRIP) $(DSTROOT)/usr/bin/screen-*
	$(RM) $(DSTROOT)/usr/bin/screen
	$(MV) $(DSTROOT)/usr/bin/screen-* $(DSTROOT)/usr/bin/screen
	$(RM) $(DSTROOT)/usr/share/info/dir

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
