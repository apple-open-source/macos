##
# Makefile for gzip
##

# Project info
Project           = gzip
UserType          = Administration
ToolType          = Commands
Extra_Configure_Flags = DEFS=NO_ASM
Extra_CC_Flags    = -mdynamic-no-pic -I/System/Library/Frameworks/System.framework/PrivateHeaders
GnuAfterInstall   = gnu_after_install install-plist install-html

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

gnu_after_install::
	$(_v) $(LN) -f $(RC_Install_Prefix)/bin/zcat $(RC_Install_Prefix)/bin/gzcat
	$(_v) $(LN) -f $(RC_Install_Prefix)/share/man/man1/zcat.1 $(RC_Install_Prefix)/share/man/man1/gzcat.1
	$(_v) $(LN) -f $(RC_Install_Prefix)/share/man/man1/zgrep.1 $(RC_Install_Prefix)/share/man/man1/zegrep.1
	$(_v) $(LN) -f $(RC_Install_Prefix)/share/man/man1/zgrep.1 $(RC_Install_Prefix)/share/man/man1/zfgrep.1

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/gnuzip.plist $(OSV)/gnuzip.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/gnuzip.txt

install-html:
	$(MKDIR) $(RC_Install_HTML)
	cd $(RC_Install_HTML) && $(TEXI2HTML) -subdir . -split_chapter $(Sources)/gzip.texi

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.3.5
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-Makefile.in patch-gzip.1 patch-gzip.c \
                 patch-zgrep.in version.diff \
                 PR4406518.diff

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
