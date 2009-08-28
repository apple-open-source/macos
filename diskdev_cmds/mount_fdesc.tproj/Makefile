Project = mount_fdesc
ProductType = tool
Install_Dir = /sbin

CFILES = mount_fdesc.c
MANPAGES = mount_fdesc.8

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip -lutil

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
