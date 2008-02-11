Project = routed
Install_Dir = /sbin

HFILES = af.h defs.h interface.h pathnames.h table.h trace.h
CFILES = af.c defs.c if.c inet.c input.c main.c output.c startup.c\
         tables.c timer.c trace.c
MANPAGES = routed.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
