##
# Makefile for enscript
##

# Project info
Project               = enscript
UserType              = Administration
ToolType              = Commands
Extra_Configure_Flags = --sysconfdir=$(Install_Prefix)/share/enscript --with-media=Letter
Extra_Install_Flags   = sysconfdir=$(DSTROOT)$(Install_Prefix)/share/enscript
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = cleanup install-plist install-man-pages

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

DUPDIRS = $(DSTROOT)/usr/lib
DUPFILES = $(DSTROOT)/usr/share/info/dir $(DSTROOT)/usr/share/locale/locale.alias

cleanup:
	$(RMDIR) $(DUPDIRS)
	$(RM) $(DUPFILES)

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

install-man-pages:
	$(MKDIR) $(DSTROOT)/$(MANDIR)/man1
	$(INSTALL_FILE) $(SRCROOT)/files/mkafmmap.1 $(DSTROOT)/$(MANDIR)/man1/mkafmmap.1
	$(INSTALL_FILE) $(SRCROOT)/files/over.1 $(DSTROOT)/$(MANDIR)/man1/over.1

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.6.4
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = manpage.diff \
		string.diff \
		enscript-CVE-2008-3863+CVE-2008-4306.patch \
		enscript-CVE-2004-1184+CVE-2004-1185+CVE-2004-1186.patch

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -zxof $(SRCROOT)/$(AEP_Filename)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -lp1 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif
