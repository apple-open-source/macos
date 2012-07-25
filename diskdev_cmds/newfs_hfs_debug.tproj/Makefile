Project = newfs_hfs_debug
ProductType = tool
Install_Dir = /usr/local/bin/

HFILES = ../newfs_hfs.tproj/hfs_endian.h ../newfs_hfs.tproj/newfs_hfs.h
CFILES = ../newfs_hfs.tproj/hfs_endian.c ../newfs_hfs.tproj/makehfs.c ../newfs_hfs.tproj/newfs_hfs.c

Extra_CC_Flags = -Wall -Werror \
	-D_LONG_LONG -Wno-four-char-constants -DDEBUG_BUILD
Extra_LD_Flags = -dead_strip -lutil
Extra_Frameworks = -framework CoreFoundation

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

ifeq ($(Embedded),NO)
Extra_Frameworks += -framework IOKit
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make


