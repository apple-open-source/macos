Project = who
Install_Dir = /usr/bin

CFILES = who.c
MANPAGES = who.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-g -O0
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
