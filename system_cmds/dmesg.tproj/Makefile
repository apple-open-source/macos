Project = dmesg
Install_Dir = /sbin

CFILES = dmesg.c
MANPAGES = dmesg.8
MAKEFILE = tool.make

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip
Extra_LD_Flags += -lproc

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
