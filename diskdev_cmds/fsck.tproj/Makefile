Project = fsck
ProductType = tool
Install_Dir = /sbin

HFILES = fsck.h ufs_byte_order.h
CFILES = dir.c ffs_subr.c ffs_tables.c inode.c main.c pass1.c\
         pass1b.c pass2.c pass3.c pass4.c pass5.c preen.c setup.c\
         ufs_byte_order.c utilities.c
MANPAGES = fsck.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-DREV_ENDIAN_FS=1 \
	-I"$(SDKROOT)"/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

#SYS_UFS_FFS_CFILES = ffs_subr.c ffs_tables.c
#OTHER_OFILES = $(SYS_UFS_FFS_CFILES:.c=.o)
#VPATH_PREAMBLE = ../XProj/sys/ufs/ffs:
