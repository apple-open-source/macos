Project = mount_hfs
ProductType = tool
Install_Dir = /sbin

CFILES = mount_hfs.c
MANPAGES = mount_hfs.8

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)
ifeq ($(Embedded),NO)
CFILES += optical.c
endif

Extra_CC_Flags = -Wall -Wno-four-char-constants
Extra_LD_Flags = -dead_strip -lutil
Extra_Frameworks = -framework IOKit -framework CoreFoundation

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
