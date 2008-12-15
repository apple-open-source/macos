Project = install
Install_Dir = /usr/bin

CFILES = xinstall.c
MANPAGES = install.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID \
	-include TargetConditionals.h
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
