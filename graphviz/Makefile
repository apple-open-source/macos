##
# Makefile for graphviz
##

# Project info
Project         = graphviz
UserType        = Developer
ToolType        = Libraries

GnuAfterInstall = install-plist
Extra_Configure_Flags = --prefix="$(Install_Prefix)" --enable-static --without-x --disable-dependency-tracking
Extra_Environment = MACOSX_DEPLOYMENT_TARGET="10.4" LIBTOOL_CMD_SEP="::"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

BuildDirectory := $(OBJROOT)/$(ProjectName)

Install_Prefix = /usr/local
Install_Man    = /usr/local/share/man

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
