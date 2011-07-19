Project = killall
Install_Dir = /usr/bin

CFILES = killall.c
MANPAGES = killall.1

Extra_CC_Flags = -Wall \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
