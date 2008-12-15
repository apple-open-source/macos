Project = newfs
ProductType = tool
Install_Dir = /sbin

HFILES = ufs_byte_order.h
CFILES = mkfs.c newfs.c ufs_byte_order.c
MANPAGES = newfs.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1 \
	-DMFS -I../disklib \
	-I"$(SDKROOT)"/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
