Project = mount_procfs
ProductType = tool
Install_Dir = /sbin

CFILES = mount_procfs.c getmntopts.c
MANPAGES = mount_procfs.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1 \
	-I../mount -DMACH_USER_API
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
