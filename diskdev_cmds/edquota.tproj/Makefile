Project = edquota
ProductType = tool
Install_Dir = /usr/sbin

HFILES = pathnames.h
CFILES = edquota.c
MANPAGES = edquota.8

Extra_CC_Flags = -Wall -Werror \
	-D_DARWIN_USE_64_BIT_INODE=1
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
