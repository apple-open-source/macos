Project = route
Install_Dir = /sbin

HFILES = keywords.h
CFILES = route.c
MANPAGES = route.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders \
	-DINET6 -DIPSEC

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
