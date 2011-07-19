Project = fs_usage
Install_Dir = /usr/bin

CFILES = fs_usage.c
MANPAGES = fs_usage.1

Extra_CC_Flags = -Wall -Werror
Extra_LD_Flags = -dead_strip -lutil

Extra_CC_Flags += -I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders\
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders/bsd

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
