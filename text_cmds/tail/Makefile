Project = tail
Install_Dir = /usr/bin

CFILES = forward.c misc.c read.c reverse.c tail.c
MANPAGES = tail.1

Extra_CC_Flags = -Wall \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
