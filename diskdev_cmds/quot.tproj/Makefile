Project = quot
ProductType = tool
Install_Dir = /usr/sbin

CFILES = quot.c
MANPAGES = quot.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
