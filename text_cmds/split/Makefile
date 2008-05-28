Project = split
Install_Dir = /usr/bin

CFILES = split.c
MANPAGES = split.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID \
	-DOFF_MAX=LLONG_MAX
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
