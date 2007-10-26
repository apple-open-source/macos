##
# Makefile for file
##

# Project info
Project               = file
UserType              = Administrator
ToolType              = Commands
Extra_Configure_Flags = --enable-fsect-man5 --disable-shared
Extra_CC_Flags        = -DBUILTIN_MACHO
GnuAfterInstall       = remove-libs install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 4.17
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = ltcf-c.sh.diff \
                 magic__Magdir__archive.diff \
                 magic__Magdir__gnu.diff \
                 magic__Magdir__mach.diff \
                 magic__Magdir__macintosh.diff \
                 magic__Magdir__sun.diff \
                 magic__Makefile.in.diff \
                 magic__magic.mime.diff \
                 src__Makefile.in.diff \
                 src__file.h.diff \
                 src__funcs.c.diff \
                 src__magic.c.diff \
                 src__readmacho.c.diff \
                 conformance.diff \
                 PR3881173.diff \
                 PR4324767.diff \
                 PR4649553.diff \
                 PR4864905.diff \
                 PR4882046.diff \
                 PR4961438.diff \
                 PR5118396.diff \
                 PR5230293.diff \
                 PR4899923.diff

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
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif

remove-libs:
	$(RMDIR) $(DSTROOT)/usr/include
	$(RMDIR) $(DSTROOT)/usr/lib
	$(RMDIR) $(DSTROOT)/usr/share/man/man3
	$(RMDIR) $(DSTROOT)/usr/share/man/man4

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LEGAL.NOTICE $(OSL)/$(Project).txt
