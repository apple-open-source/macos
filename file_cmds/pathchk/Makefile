Project = pathchk
Install_Dir = /usr/bin

CFILES = pathchk.c
MANPAGES = pathchk.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID 
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
