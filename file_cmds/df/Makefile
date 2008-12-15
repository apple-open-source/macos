Project = df
Install_Dir = /bin

CFILES = df.c vfslist.c
MANPAGES = df.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID \
	"-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders" \
	-D_DARWIN_USE_64_BIT_INODE
Extra_LD_Flags = -dead_strip \
	-lutil

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
