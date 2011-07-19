Project = rlogind
Install_Dir = /usr/libexec

HFILES = pathnames.h
CFILES = rlogind.c
MANPAGES = rlogind.8
LAUNCHD_PLISTS = login.plist

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_LD_Flags = -dead_strip -pie

Extra_CC_Flags += -D__FBSDID=__RCSID

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	plutil -convert binary1 "$(DSTROOT)/System/Library/LaunchDaemons/$(LAUNCHD_PLISTS)"
