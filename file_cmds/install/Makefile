Project = install
Install_Dir = /usr/bin

CFILES = xinstall.c
MANPAGES = install.1

Extra_CC_Flags = -Werror -Wall -mdynamic-no-pic -D__FBSDID=__RCSID \
	-include TargetConditionals.h -D_DARWIN_USE_64_BIT_INODE
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
