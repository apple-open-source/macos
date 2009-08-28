Project = clri
ProductType = tool
Install_Dir = /sbin

HFILES = ufs_byte_order.h
CFILES = clri.c ufs_byte_order.c
MANPAGES = clri.8

Extra_CC_Flags = -Wall -mdynamic-no-pic -DREV_ENDIAN_FS=1
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
