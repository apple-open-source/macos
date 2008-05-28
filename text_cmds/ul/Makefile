Project = ul
Install_Dir = /usr/bin

CFILES = ul.c
MANPAGES = ul.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip \
	-lcurses

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
