Project = vsdbutil
ProductType = tool
Install_Dir = /usr/sbin

CFILES = vsdbutil_main.c
MANPAGES = vsdbutil.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
