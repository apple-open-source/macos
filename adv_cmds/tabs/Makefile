Project = tabs
Install_Dir = /usr/bin

CFILES = tabs.c
MANPAGES = tabs.1
Extra_CC_Flags = -mdynamic-no-pic -Wall -Werror -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip
Extra_LD_Flags += -ltermcap

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
