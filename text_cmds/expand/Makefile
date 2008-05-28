Project = expand
Install_Dir = /usr/bin

CFILES = expand.c
MANPAGES = expand.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)/usr/share/man/man1/expand.1 \
		 $(DSTROOT)/usr/share/man/man1/unexpand.1
