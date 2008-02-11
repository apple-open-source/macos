Project = locate.code
Install_Dir = /usr/libexec

CFILES = locate.code.c

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-I../locate
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
