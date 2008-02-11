Project = date
Install_Dir = /bin

HFILES = extern.h vary.h
CFILES = date.c netdate.c vary.c
MANPAGES = date.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
