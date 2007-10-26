##
# Makefile for glibtool
##

# Project info
Project               = glibtool
UserType              = Developer
ToolType              = Commands
Extra_Configure_Flags = --program-prefix="g"
GnuAfterInstall	      = install-man-pages install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

MAN1 = $(DSTROOT)/usr/share/man/man1

install-man-pages:
	$(MKDIR) $(MAN1)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).1 $(MAN1)
	$(LN) $(MAN1)/$(Project).1 $(MAN1)/glibtoolize.1
