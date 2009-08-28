Project = cksum
Install_Dir = /usr/bin

CFILES = cksum.c crc.c print.c sum1.c sum2.c crc32.c
MANPAGES = cksum.1

Extra_CC_Flags = -Werror -Wall -mdynamic-no-pic -D__FBSDID=__RCSID -D_DARWIN_USE_64_BIT_INODE
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)$(Install_Dir)/cksum \
		$(DSTROOT)$(Install_Dir)/sum
	$(LN) -f $(DSTROOT)/usr/share/man/man1/cksum.1 \
		$(DSTROOT)/usr/share/man/man1/sum.1
