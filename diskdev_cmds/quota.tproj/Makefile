Project = quota
ProductType = tool
Install_Dir = /usr/bin

CFILES = quota.c
MANPAGES = quota.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
