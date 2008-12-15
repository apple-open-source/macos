Project = mknod
Install_Dir = /sbin

CFILES = mknod.c
MANPAGES = mknod.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
