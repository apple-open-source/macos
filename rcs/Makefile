##
# Makefile for RCS
##

# Project info
Project               = rcs
UserType              = Developer
ToolType              = Commands
Extra_Configure_Flags = --with-diffutils
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

