##
# Makefile for grep
##

# Project info
Project               = grep
UserType              = Administration
ToolType              = Commands
Extra_Configure_Flags = --disable-nls
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = install-html install-plist fix-egrep install-symbol

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Fix for 3930638.
fix-egrep:
	$(RM) $(DSTROOT)/usr/bin/egrep
	$(LN) $(DSTROOT)/usr/bin/grep $(DSTROOT)/usr/bin/egrep
	$(RM) $(DSTROOT)/usr/bin/fgrep
	$(LN) $(DSTROOT)/usr/bin/grep $(DSTROOT)/usr/bin/fgrep

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 2.5.1
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
Fedora_Patches = grep-2.5.1-fgrep.patch grep-2.5.1-bracket.patch grep-2.5-i18n.patch grep-2.5.1-egf-speedup.patch
AEP_Patches    = doc__Makefile.in.diff doc__grep.1.diff src__dfa.c.diff PR-3715846.diff PR-3716425.diff \
	PR-3716570.diff PR-3934152.diff PR-4053512.diff src__search.c.diff

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
	for patchfile in $(Fedora_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p1 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
endif

install-html:
	$(MAKE) -C $(BuildDirectory)/doc $(Environment) version.texi
	$(MKDIR) $(RC_Install_HTML)
	cd $(RC_Install_HTML) && $(TEXI2HTML) -subdir . -split chapter \
		-I $(BuildDirectory)/doc $(Sources)/doc/grep.texi

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

install-symbol:
	$(CP) $(OBJROOT)/src/grep $(SYMROOT)
