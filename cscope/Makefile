##
# Makefile for cscope
# wsanchez@opensource.apple.com
##

# Project info
Project               = cscope
UserType              = Developer
ToolType              = Commands
Extra_Environment     =   AUTOCONF="$(Sources)/missing autoconf"	\
		        AUTOHEADER="$(Sources)/missing autoheader"
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/cscope.plist $(OSV)
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/cscope/COPYING $(OSL)/$(Project).txt
