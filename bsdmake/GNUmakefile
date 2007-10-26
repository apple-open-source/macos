##
# Makefile for BSD make
##

# Project info
Project  = bsdmake
UserType = Developer
ToolType = Commands

Extra_CC_Flags = -D__FBSDID=__RCSID -mdynamic-no-pic
Extra_Environment = WARNS=1

BSD_After_Install = munge_name install_mk install_plist

# It's a BSD project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

BSDMAKE += -m $(SRCROOT)/mk

install_headers:: install_mk

munge_name:
	$(_v) $(MV) $(DSTROOT)$(USRBINDIR)/make $(DSTROOT)$(USRBINDIR)/bsdmake
	$(_v) $(MV) $(DSTROOT)$(MANDIR)/man1/make.1.gz $(DSTROOT)$(MANDIR)/man1/bsdmake.1.gz

install_mk:
	@echo "Installing $(Project)/mk..."
	$(INSTALL_DIRECTORY) "$(DSTROOT)$(SHAREDIR)/mk"
	$(_v) umask $(Install_Mask) ;					\
		cd mk && $(Environment) MAKEOBJDIR="$(SRCROOT)/mk"	\
			 $(BSDMAKE) $(Install_Environment) BINDIR="$(SHAREDIR)" $(Install_Target)

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install_plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(HEAD) -n 30 $(SRCROOT)/make.1 > $(OSL)/$(Project).txt
