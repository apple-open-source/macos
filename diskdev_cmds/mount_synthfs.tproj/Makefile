Project = mount_synthfs
ProductType = tool
Install_Dir = /sbin

CFILES = mount_synthfs.c

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
