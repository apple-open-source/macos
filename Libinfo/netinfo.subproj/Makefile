Project = netinfo
ProductType = staticlib
Install_Dir = /scratch
BuildDebug = YES
BuildProfile = YES

CFILES = ni_stub.c

Extra_CC_Flags = -Wall -fno-common \
	-D__DARWIN_NON_CANCELABLE=1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
