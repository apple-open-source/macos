Project = quotacheck
ProductType = tool
Install_Dir = /sbin

HFILES = quotacheck.h ufs_byte_order.h
CFILES = quotacheck.c ufs_quotacheck.c ufs_byte_order.c hfs_quotacheck.c
MANPAGES = quotacheck.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic \
	-DREV_ENDIAN_FS=1
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
