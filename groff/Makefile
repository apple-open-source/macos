##
# Makefile for groff
##

# Project info
Project             = groff
UserType            = Administrator
ToolType            = Commands
Extra_CC_Flags      = -mdynamic-no-pic
Extra_Install_Flags = INSTALL_PROGRAM="$(INSTALL) -c -s"
GnuAfterInstall     = symlink remove-dir install-plist

Extra_Configure_Flags = --without-x

# GNU build setup
install:: makeprefix
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
Install_Target = install

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.19.2
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = tmac__doc-common.diff tmac__troffrc.diff \
                 tmac__doc-syms.diff tmac__groff_mdoc.man.diff \
                 utf8.diff \
                 PR-13280133.diff

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

# Satisfy bogus check during installation.
makeprefix:
	mkdir -p $(DSTROOT)/usr

# Create links for "missing" manpages.
symlink:
	$(LN) $(DSTROOT)$(MANDIR)/man1/grohtml.1 $(DSTROOT)$(MANDIR)/man1/pre-grohtml.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/grohtml.1 $(DSTROOT)$(MANDIR)/man1/post-grohtml.1
	$(LN) $(DSTROOT)$(MANDIR)/man7/groff_mdoc.7 $(DSTROOT)$(MANDIR)/man7/mdoc.7

# Remove the info/dir file.
remove-dir:
	rm $(DSTROOT)/usr/share/info/dir

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
