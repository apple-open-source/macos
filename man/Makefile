##
# Makefile for mandoc
##

# Project info
Project		      = man
ProjectVersion	      = 20210109

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

include $(CoreOSMakefiles)/ReleaseControl/Common.make

Sources = $(SRCROOT)/$(Project)

install:: install-license install-plist
	@echo "Installing $(Project)..."
	$(_v) $(MAKE) -C ${Sources} DESTDIR=$(DSTROOT) install

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist

install-license:
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).txt $(OSL)/$(Project).txt
