Project = quotaon
ProductType = tool
Install_Dir = /usr/sbin

CFILES = quotaon.c
MANPAGES = quotaon.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f "$(DSTROOT)$(Install_Dir)"/quotaon "$(DSTROOT)$(Install_Dir)"/quotaoff
	$(LN) -f "$(DSTROOT)"/usr/share/man/man8/quotaon.8 "$(DSTROOT)"/usr/share/man/man8/quotaoff.8
