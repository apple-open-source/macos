Project = fsck
ProductType = tool
Install_Dir = /sbin

HFILES = fsck.h 
CFILES = fsck.c
MANPAGES = fsck.8

Extra_CC_Flags = -Wall -Wuninitialized -Wunused -Werror\
    -I"$(SDKROOT)"/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -dead_strip 

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

