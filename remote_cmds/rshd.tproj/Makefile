Project = rshd
Install_Dir = /usr/libexec

CFILES = rshd.c
MANPAGES = rshd.8
LAUNCHD_PLISTS = shell.plist

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)
ifeq ($(Embedded),YES)
PAM=
else
PAM=-lpam
endif
Extra_LD_Flags = -dead_strip -pie $(PAM)

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make


after_install:
ifeq ($(Embedded),YES)
	/usr/libexec/PlistBuddy -x \
		-c "Delete :Disabled" \
		-c "Delete :SessionCreate" \
		-c "Add :Sockets:Listeners:SockNodeName string localhost" \
		"$(DSTROOT)/System/Library/LaunchDaemons/shell.plist"
endif
	plutil -convert binary1 "$(DSTROOT)/System/Library/LaunchDaemons/$(LAUNCHD_PLISTS)"
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/private/etc/pam.d
	$(INSTALL_FILE) rshd.pam "$(DSTROOT)"/private/etc/pam.d/rshd
