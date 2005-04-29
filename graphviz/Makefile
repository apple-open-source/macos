##
# Makefile for graphviz
##

# Project info
Project         = graphviz
UserType        = Developer
ToolType        = Libraries

PSEUDO_DSTROOT  = $(subst ~,-,$(DSTROOT))

GnuAfterInstall = after_install install-plist
Extra_CC_Flags  = -mtune=G5 -Wno-long-double
Extra_Configure_Flags = --prefix="$(Install_Prefix)" --enable-static --without-x
Extra_Environment = MAKEFLAGS="-j 2" DESTDIR="$(PSEUDO_DSTROOT)" MACOSX_DEPLOYMENT_TARGET="10.4"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

BuildDirectory := $(OBJROOT)/$(ProjectName)

Install_Prefix = /usr/local
Install_Man    = /usr/local/share/man

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

after_install:
	if [ "$(PSEUDO_DSTROOT)" != "$(DSTROOT)" ]; then \
	    $(CP) -pR $(PSEUDO_DSTROOT)/ $(DSTROOT); \
	fi

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
