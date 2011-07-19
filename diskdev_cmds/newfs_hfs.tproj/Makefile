Project = newfs_hfs
ProductType = tool
Install_Dir = /sbin

HFILES = hfs_endian.h newfs_hfs.h
CFILES = hfs_endian.c makehfs.c newfs_hfs.c
MANPAGES = newfs_hfs.8

Extra_CC_Flags = -Wall -Werror \
	-D_LONG_LONG -Wno-four-char-constants
Extra_LD_Flags = -dead_strip -lutil
Extra_Frameworks = -framework CoreFoundation

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

ifeq ($(Embedded),NO)
Extra_Frameworks += -framework IOKit
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

