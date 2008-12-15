Project = ls
Install_Dir = /bin

CFILES = cmp.c ls.c print.c util.c humanize_number.c
MANPAGES = ls.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID \
	"-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders" \
	-DCOLORLS \
	-D_DARWIN_USE_64_BIT_INODE \
	-include TargetConditionals.h
Extra_LD_Flags = -dead_strip \
	-lncurses

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
