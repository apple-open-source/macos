##
# Makefile for nano
##

# Project info
Project         = nano
UserType        = Administration
ToolType        = Commands
GnuAfterInstall = remove-junk install-plist link-pico install-nanorc

Extra_CC_Flags        = -mdynamic-no-pic
Extra_Configure_Flags = --sysconfdir="$(ETCDIR)" --enable-all

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 2.0.6
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = configure.diff

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

remove-junk:
	$(RM) $(DSTROOT)/usr/share/info/dir
	$(RM) $(DSTROOT)/usr/bin/rnano
	$(RM) $(DSTROOT)/usr/share/man/man1/rnano.1
	$(RMDIR) $(DSTROOT)/usr/share/nano

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

link-pico:
	$(LN) -s nano $(DSTROOT)/usr/bin/pico
	$(LN) -s nano.1 $(DSTROOT)/usr/share/man/man1/pico.1

install-nanorc:
	$(MKDIR) $(DSTROOT)$(ETCDIR)
	$(INSTALL_FILE) $(SRCROOT)/nanorc $(DSTROOT)$(ETCDIR)/nanorc
