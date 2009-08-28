Project = msgs
Install_Dir = /usr/bin

CFILES = msgs.c
MANPAGES = msgs.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip \
	-lcurses

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
