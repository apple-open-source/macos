Project = trpt
Install_Dir = /usr/sbin

CFILES = trpt.c
MANPAGES = trpt.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -lcurses

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 02555
Install_Program_Group = kmem
