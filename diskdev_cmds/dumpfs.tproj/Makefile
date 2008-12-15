Project = dumpfs
ProductType = tool
Install_Dir = /sbin

HFILES = ufs_byte_order.h
CFILES = dumpfs.c ufs_byte_order.c
MANPAGES = dumpfs.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1 \
	-DREV_ENDIAN_FS=1
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
