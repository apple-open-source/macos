Project = wall
Install_Dir = /usr/bin

HFILES = ttymsg.h
CFILES = ttymsg.c wall.c
MANPAGES = wall.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

Extra_CC_Flags += -D__FBSDID=__RCSID

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 02555
Install_Program_Group = tty
