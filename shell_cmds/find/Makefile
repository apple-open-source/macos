Project = find
Install_Dir = /usr/bin

HFILES = extern.h find.h
CFILES = find.c function.c ls.c main.c misc.c operator.c option.c
YFILES = getdate.y
MANPAGES = find.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic \
	-D__FBSDID=__RCSID -D_DARWIN_USE_64_BIT_INODE
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
