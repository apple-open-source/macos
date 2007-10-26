##
# Makefile for enscript
##

# Project info
Project               = enscript
UserType              = Administration
ToolType              = Commands
Extra_Configure_Flags = --sysconfdir=$(Install_Prefix)/share/enscript --with-media=Letter
Extra_Install_Flags   = sysconfdir=$(DSTROOT)$(Install_Prefix)/share/enscript
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = install-plist install-man-pages

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

install-man-pages:
	$(MKDIR) $(DSTROOT)/$(MANDIR)/man1
	$(INSTALL_FILE) $(SRCROOT)/files/mkafmmap.1 $(DSTROOT)/$(MANDIR)/man1/mkafmmap.1
	$(INSTALL_FILE) $(SRCROOT)/files/over.1 $(DSTROOT)/$(MANDIR)/man1/over.1
