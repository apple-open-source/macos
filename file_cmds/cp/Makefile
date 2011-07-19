Project = cp
Install_Dir = /bin

CFILES = cp.c utils.c
MANPAGES = cp.1

Extra_CC_Flags = -Werror -Wall -D__FBSDID=__RCSID -D_DARWIN_USE_64_BIT_INODE
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
