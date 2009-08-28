Project = mount_union
ProductType = tool
Install_Dir = /sbin

CFILES = mount_union.c
MANPAGES = mount_union.8

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip -lutil

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
