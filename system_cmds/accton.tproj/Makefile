Project = accton
Install_Dir = /usr/sbin

CFILES = accton.c
MANPAGES = accton.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
