Project = mount_devfs
ProductType = tool
Install_Dir = /sbin

CFILES = mount_devfs.c

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip -lutil

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
