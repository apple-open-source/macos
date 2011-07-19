Project = stat
Install_Dir = /usr/bin

CFILES = stat.c
MANPAGES = stat.1

Extra_CC_Flags = -Werror -Wall -D__FBSDID=__RCSID -D_DARWIN_USE_64_BIT_INODE
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)$(Install_Dir)/stat \
		 $(DSTROOT)$(Install_Dir)/readlink
	$(LN) -f $(DSTROOT)/usr/share/man/man1/stat.1 \
		 $(DSTROOT)/usr/share/man/man1/readlink.1
