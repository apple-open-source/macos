Project = ipcs
Install_Dir = /usr/bin

CFILES = ipcs.c
MANPAGES = ipcs.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
        -D__FBSDID=__RCSID \
	-iquote \
	"$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders" \
	-iquote \
	"$(SDKROOT)/System/Library/Frameworks/Kernel.framework/PrivateHeaders"
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04511
