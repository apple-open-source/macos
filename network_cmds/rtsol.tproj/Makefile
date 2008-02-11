Project = rtsol
Install_Dir = /sbin

CFILES = rtsold.c rtsol.c if.c probe.c dump.c rtsock.c
HFILES = rtsold.h
MANPAGES = rtsol.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-DINET6 -DHAVE_GETIFADDRS \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -lipsec

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/usr/sbin
	$(LN) -f "$(DSTROOT)"/sbin/rtsol "$(DSTROOT)"/usr/sbin/rtsold
	$(LN) -f "$(DSTROOT)"/usr/share/man/man8/rtsol.8 \
		 "$(DSTROOT)"/usr/share/man/man8/rtsold.8
