Project = rmt
Install_Dir = /usr/sbin

CFILES = rmt.c
MANPAGES = rmt.8

Extra_CC_Flags = -Werror -Wall -mdynamic-no-pic -D__FBSDID=__RCSID -D_DARWIN_USE_64_BIT_INODE
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
