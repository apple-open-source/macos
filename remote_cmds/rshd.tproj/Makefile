Project = rshd
Install_Dir = /usr/libexec

CFILES = rshd.c
MANPAGES = rshd.8
LAUNCHD_PLISTS = shell.plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

after_install:
ifeq ($(Embedded),YES)
	/usr/libexec/PlistBuddy -x \
		-c "Delete :Disabled" \
		-c "Delete :SessionCreate" \
		"$(DSTROOT)/System/Library/LaunchDaemons/shell.plist"
endif
