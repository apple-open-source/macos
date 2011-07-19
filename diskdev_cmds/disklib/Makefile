Project = disk
ProductType = staticlib
Install_Dir = /usr/local/lib

HFILES = mntopts.h pathnames.h dkopen.h

CFILES = dkcksum.c dksecsize.c preen.c\
         vfslist.c dkdisklabel.c  dkopen.c

Extra_CC_Flags = -Wall  \
	-I"$(SDKROOT)"/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -dead_strip

PROJECT_HEADERS = pathnames.h

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
