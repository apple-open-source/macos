Project = sleep
Install_Dir = /bin

CFILES = sleep.c
MANPAGES = sleep.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
