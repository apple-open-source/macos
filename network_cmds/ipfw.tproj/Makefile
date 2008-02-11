Project = ipfw
Install_Dir = /sbin

CFILES = ipfw2.c
MANPAGES = ipfw.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

