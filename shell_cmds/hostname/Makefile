Project = hostname
Install_Dir = /bin

CFILES = hostname.c
MANPAGES = hostname.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-DKERNEL_PRIVATE
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
