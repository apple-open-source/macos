##
# Makefile for srm
##

# Project info
Project     = srm
ProjectName = srm
ToolType    = Commands
CFLAGS = -g
GnuAfterInstall = install-plist dsym

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(OSV)/$(ProjectName).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(ProjectName).txt

dsym: $(OBJROOT)/src/srm
	$(MKDIR) -p "$(SYMROOT)"
	$(CP) "$(OBJROOT)/src/srm" "$(SYMROOT)/srm"
	dsymutil "$(SYMROOT)/srm" --out="$(SYMROOT)/srm.dSYM"

