##
# Makefile for Patch
##

# Project info
Project        = patch
UserType       = Developer
ToolType       = Commands
Extra_CC_Flags = -mdynamic-no-pic
GnuAfterInstall	= install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/patch_cmds.plist $(OSV)/patch_cmds.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/patch_cmds.txt
