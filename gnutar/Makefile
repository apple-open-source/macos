##
# gnutar Makefile
##

# Project info
Project               = gnutar
UserType              = Administrator
ToolType              = Commands
Extra_Configure_Flags = --program-prefix=gnu --includedir=/usr/local/include
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = remove-junk install-symlink install-plist

Install_Prefix  = /usr/local
Install_Info    = /usr/local/share/info

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = tar
AEP_Version    = 1.17
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = Makefile.in.diff tar-1.17-buildfix.diff \
                 EA.diff preallocate.diff quarantine.diff \
                 PR5405409.diff PR5605786.diff PR6450027.diff \
                 PR7691662.diff

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
	@for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 -F0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif

remove-junk:
	$(RMDIR) $(DSTROOT)$(Install_Prefix)/lib/
	$(RMDIR) $(DSTROOT)$(Install_Prefix)/libexec/
	$(RMDIR) $(DSTROOT)$(Install_Prefix)/sbin/
	$(RMDIR) $(DSTROOT)$(Install_Prefix)/share/

install-symlink:
	$(MKDIR) $(DSTROOT)/usr/bin/
	$(LN) -fs $(Install_Prefix)/bin/gnutar $(DSTROOT)/usr/bin/gnutar

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
