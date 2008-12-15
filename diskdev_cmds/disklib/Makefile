Project = disk
ProductType = staticlib
Install_Dir = /usr/local/lib

HFILES = dump.h fsck.h mntopts.h pathnames.h ufslabel.h dkopen.h

CFILES = dkcksum.c dksecsize.c dumprmt.c getmntopts.c preen.c\
         vfslist.c dkdisklabel.c ufslabel.c dkopen.c

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1 \
	-I"$(SDKROOT)"/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -dead_strip

PROJECT_HEADERS = fsck.h pathnames.h dump.h

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
