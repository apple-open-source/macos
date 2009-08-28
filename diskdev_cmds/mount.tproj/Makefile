Project = mount
ProductType = tool
Install_Dir = /sbin

HFILES = pathnames.h
CFILES = mount.c mount_ufs.c
MANPAGES = mount.8 fstab.5

Extra_CC_Flags = -Wall -mdynamic-no-pic -Wno-error=deprecated-declarations
Extra_LD_Flags = -dead_strip \
	-lutil \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
