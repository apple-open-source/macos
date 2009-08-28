Project = mount_umap
ProductType = tool
Install_Dir = /sbin

CFILES = mount_umap.c
MANPAGES = mount_umap.8

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
