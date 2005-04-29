# Set these variables as needed, then include this file, then:
#

# Project info
Project         = texinfo
UserType        = Documentation
ToolType        = Services
GnuAfterInstall = fix-locale install-dir install-plist
Extra_CC_Flags  = -mdynamic-no-pic

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# texinfo installs some locale information in a way which conflicts with other
# projects.  Conform to the standard.
fix-locale:
	$(MV) $(DSTROOT)/usr/share/locale/de_AT $(DSTROOT)/usr/share/locale/de_AT.ISO8859-1
	$(RM) $(DSTROOT)/usr/lib/charset.alias

install-dir:
	$(MKDIR) $(DSTROOT)/usr/share/info
	$(INSTALL_FILE) $(SRCROOT)/dir $(DSTROOT)/usr/share/info/dir

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(OBJROOT)/COPYING $(OSL)/$(Project).txt

