Project = join
Install_Dir = /usr/bin

CFILES = join.c
MANPAGES = join.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
