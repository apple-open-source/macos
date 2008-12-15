Project = rmt
Install_Dir = /usr/sbin

CFILES = rmt.c
MANPAGES = rmt.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
        -D__FBSDID=__RCSID 
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
