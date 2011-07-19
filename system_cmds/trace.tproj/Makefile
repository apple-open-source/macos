Project = trace
Install_Dir = /usr/bin

CFILES = trace.c
MANPAGES = trace.1

Extra_CC_Flags =
Extra_LD_Flags = -dead_strip

Extra_CC_Flags += -I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders\
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders/bsd
Extra_LD_Flags += -lutil

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
