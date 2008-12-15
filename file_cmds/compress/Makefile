Project = compress
Install_Dir = /usr/bin

CFILES = compress.c zopen.c
MANPAGES = compress.1 uncompress.1 zopen.3

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)$(Install_Dir)/compress \
                 $(DSTROOT)$(Install_Dir)/uncompress
