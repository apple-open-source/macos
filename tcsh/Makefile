##
# Makefile for tcsh
##

# Project info
Project               = tcsh
UserType              = Administration
ToolType              = Commands
Extra_CC_Flags        = -D_PATH_TCSHELL='\"/bin/tcsh\"' -no-cpp-precomp -mdynamic-no-pic -DDARWIN -fstack-protector-all
Extra_Configure_Flags = --bindir="/bin"
Extra_Install_Flags   = DESTBIN="$(DSTROOT)/bin" MANSECT="1" DESTMAN="$(DSTROOT)/usr/share/man/man1" srcdir="$(SRCROOT)/tcsh"
GnuAfterInstall       = install-links install-rc install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install install.man

install-rc:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/$(ETCDIR)
	$(_V) $(INSTALL) -c -m 0644 -o root -g wheel $(SRCROOT)/csh.cshrc $(DSTROOT)/$(ETCDIR)/
	$(_V) $(INSTALL) -c -m 0644 -o root -g wheel $(SRCROOT)/csh.login $(DSTROOT)/$(ETCDIR)/
	$(_V) $(INSTALL) -c -m 0644 -o root -g wheel $(SRCROOT)/csh.logout $(DSTROOT)/$(ETCDIR)/

install-links:
	$(_v) $(CP) $(OBJROOT)/tcsh $(SYMROOT)/tcsh
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(BINDIR)
	$(_v) $(LN) -f $(DSTROOT)$(BINDIR)/tcsh $(DSTROOT)$(BINDIR)/csh
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/man/man1
	$(_v) $(LN) -f $(DSTROOT)/usr/share/man/man1/tcsh.1 \
		$(DSTROOT)/usr/share/man/man1/csh.1

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(_v) $(INSTALL_DIRECTORY) $(OSV)
	$(_v) $(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(_v) $(INSTALL_DIRECTORY) $(OSL)
	$(_v) $(INSTALL_FILE) $(SRCROOT)/$(Project)/Copyright $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 6.17.00
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = config_f.h.diff host.defs.patch tc.sig.h.patch


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
		cd $(SRCROOT)/$(Project) && patch -p0 -F0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
endif
