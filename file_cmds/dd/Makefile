Project = dd
Install_Dir = /bin

CFILES = args.c conv.c conv_tab.c dd.c misc.c position.c
MANPAGES = dd.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
