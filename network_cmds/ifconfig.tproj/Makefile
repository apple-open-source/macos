Project = ifconfig
Install_Dir = /sbin

HFILES = ifconfig.h
CFILES = ifconfig.c ifmedia.c ifvlan.c ifbond.c
MANPAGES = ifconfig.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders \
	-DUSE_IF_MEDIA -DINET6 -DNO_IPX -DUSE_VLANS -DUSE_BONDS

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
