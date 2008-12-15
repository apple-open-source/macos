Project = mount_cd9660
ProductType = tool
Install_Dir = /sbin

CFILES = mount_cd9660.c
MANPAGES = mount_cd9660.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
        -D_DARWIN_USE_64_BIT_INODE=1
Extra_LD_Flags = -dead_strip -lutil
Extra_Frameworks = -framework IOKit -framework CoreFoundation

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
