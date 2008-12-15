Project = aslmanager
ProductType = tool
Install_Dir = /usr/sbin

CFILES = aslmanager.c
MANPAGES = aslmanager.8
LAUNCHD_PLISTS = com.apple.aslmanager.plist

Extra_CC_Flags = -Wall -mdynamic-no-pic -DINET6
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
