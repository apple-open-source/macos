Project = nologin
Install_Dir = /sbin

CFILES = nologin.c
MANPAGES = nologin.5 nologin.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
