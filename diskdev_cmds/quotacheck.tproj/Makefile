Project = quotacheck
ProductType = tool
Install_Dir = /sbin

HFILES = quotacheck.h 
CFILES = quotacheck.c hfs_quotacheck.c
MANPAGES = quotacheck.8

Extra_CC_Flags = -Wall -Werror 

Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
