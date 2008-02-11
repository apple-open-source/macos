Project = wall
Install_Dir = /usr/bin

HFILES = ttymsg.h
CFILES = ttymsg.c wall.c
MANPAGES = wall.1

Extra_CC_Flags = -D__FBSDID=__RCSID

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 02555
Install_Program_Group = tty
