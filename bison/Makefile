##
# Makefile for Bison
##

# Project info
Project         = bison
UserType        = Developer
ToolType        = Commands
GnuAfterInstall = cleanup install-plist install-yacc
Extra_CC_Flags  = -mdynamic-no-pic

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

cleanup:
	$(RM) $(DSTROOT)/usr/bin/yacc
	$(RMDIR) $(DSTROOT)/usr/lib
	$(RM) $(DSTROOT)/usr/share/info/dir

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/bison.plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

install-yacc:
	$(INSTALL_SCRIPT) $(SRCROOT)/yacc.sh $(DSTROOT)/usr/bin/yacc
	$(INSTALL_FILE) $(SRCROOT)/yacc.1 $(DSTROOT)/usr/share/man/man1/yacc.1

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 2.3
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = relative_path.diff PR4669094.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
