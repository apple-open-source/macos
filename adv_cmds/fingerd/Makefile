Project = fingerd
Install_Dir = /usr/libexec

CFILES = fingerd.c
MANPAGES = fingerd.8
LAUNCHD_PLISTS = finger.plist

Extra_CC_Flags = -fPIE -Wall -Werror
Extra_LD_Flags = -dead_strip -pie -lutil

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
