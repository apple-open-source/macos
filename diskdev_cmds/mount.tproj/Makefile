Project = mount
ProductType = tool
Install_Dir = /sbin

HFILES = pathnames.h
CFILES = mount.c 
MANPAGES = mount.8 fstab.5

Extra_CC_Flags = -Wall -Wno-error=deprecated-declarations
Extra_LD_Flags = -dead_strip \
	-lutil \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
