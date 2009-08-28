Project = ruptime
Install_Dir = /usr/bin

CFILES = ruptime.c
MANPAGES = ruptime.1

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
