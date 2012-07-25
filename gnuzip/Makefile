##
# Makefile for gzip
##

# Project info
Project           = gzip
UserType          = Administration
ToolType          = Commands
Extra_Configure_Flags = DEFS=NO_ASM
Extra_CC_Flags    = -DGNU_STANDARD=0 -mdynamic-no-pic
GnuAfterInstall   = gnu_after_install install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

gnu_after_install::
	$(_v) $(LN) -f $(RC_Install_Prefix)/bin/zcat $(RC_Install_Prefix)/bin/gzcat
	$(_v) $(LN) -f $(RC_Install_Prefix)/share/man/man1/zcat.1 $(RC_Install_Prefix)/share/man/man1/gzcat.1
	$(RM) $(DSTROOT)/usr/bin/uncompress
	$(_v) $(LN) -f $(RC_Install_Prefix)/bin/gzip $(RC_Install_Prefix)/bin/zcat

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/gnuzip.plist $(OSV)/gnuzip.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/gnuzip.txt

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 1.3.12
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-doc__Makefile.in \
                 patch-gzip.1 patch-gzip.c \
                 patch-zless.1 \
                 PR-7408343.diff \
                 PR-7634893.diff \
                 remove-zgrep.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -F0 -p0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
