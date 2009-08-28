Project = atrun
Install_Dir = /usr/libexec

HFILES = atrun.h
CFILES = atrun.c
MANPAGES = atrun.8
LAUNCHD_PLISTS = com.apple.atrun.plist

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_CC_Flags += -DDAEMON_UID=1 -DDAEMON_GID=1 -I../at.tproj
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
