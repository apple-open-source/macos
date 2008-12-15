Project = ln
Install_Dir = /bin

CFILES = ln.c
MANPAGES = ln.1 symlink.7

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)$(Install_Dir)/ln \
		$(DSTROOT)$(Install_Dir)/link
	$(LN) -f $(DSTROOT)/usr/share/man/man1/ln.1 \
		$(DSTROOT)/usr/share/man/man1/link.1
