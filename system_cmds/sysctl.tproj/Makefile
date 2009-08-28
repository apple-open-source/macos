Project = sysctl
Install_Dir = /usr/sbin

CFILES = sysctl.c
MANPAGES = sysctl.8 sysctl.conf.5

Extra_CC_Flags = -mdynamic-no-pic -Wall -Werror
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
