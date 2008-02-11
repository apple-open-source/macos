Project = latency
Install_Dir = /usr/bin

CFILES = latency.c
MANPAGES = latency.1

Extra_LD_Flags = -lcurses

Extra_CC_Flags = -I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders\
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders/bsd

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
