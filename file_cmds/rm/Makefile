Project = rm
Install_Dir = /bin

CFILES = rm.c
MANPAGES = rm.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID 
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)$(Install_Dir)/rm \
		$(DSTROOT)$(Install_Dir)/unlink
	$(LN) -f $(DSTROOT)/usr/share/man/man1/rm.1 \
		$(DSTROOT)/usr/share/man/man1/unlink.1
