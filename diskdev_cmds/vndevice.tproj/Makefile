Project = vndevice
ProductType = tool
Install_Dir = /usr/libexec

CFILES = vndevice.c

Extra_CC_Flags = -Wall \
	-I"$(SDKROOT)"/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
