Project = bootlog
Install_Dir = /usr/libexec

CFILES = bootlog.c
MANPAGES = bootlog.8
LAUNCHD_PLISTS = com.apple.bootlog.plist

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	plutil -convert binary1 "$(DSTROOT)/System/Library/LaunchDaemons/$(LAUNCHD_PLISTS)"
