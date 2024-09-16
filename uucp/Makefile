##
# Makefile for uucp
## 

# Project info
Project           = uucp
UserType          = Developer
ToolType          = Commands
GnuNoChown=YES
GnuAfterInstall     = post-install install-plist
Extra_Configure_Flags= --with-newconfigdir=/private/etc/uucp --with-user=_uucp

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Directories that will be created and owned by _uucp
UUCP_DIRS=$(DSTROOT)/private/var/log/uucp $(DSTROOT)/private/var/spool/uucp

post-install:
	$(MKDIR) $(UUCP_DIRS) $(DSTROOT)/private/etc/uucp
	$(INSTALL_FILE) $(SRCROOT)/configs/passwd  $(DSTROOT)/private/etc/uucp
	$(INSTALL_FILE) $(SRCROOT)/configs/port    $(DSTROOT)/private/etc/uucp
	$(INSTALL_FILE) $(SRCROOT)/configs/sys     $(DSTROOT)/private/etc/uucp
	$(CHOWN) _uucp $(UUCP_DIRS)

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL_FILE) $(SRCROOT)/com.apple.uucp.plist $(DSTROOT)/System/Library/LaunchDaemons
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/uucp.plist $(OSV)/uucp.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/uucp.txt
	# there isn't a configure option to not setuid a bunch of stuff, so we let it do that and then we "fix it"
	chmod -R ug-s $(DSTROOT)/usr/*bin


# Extract the source.
install_source::


install::
	chgrp -R wheel $(DSTROOT)
	rm -rf $(DSTROOT)/usr/share/info
