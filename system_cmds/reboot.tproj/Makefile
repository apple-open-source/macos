Project = reboot
Install_Dir = /sbin

CFILES = reboot.c
MANPAGES = reboot.8

Embedded = $(shell tconf --test TARGET_OS_EMBEDDED)
ifeq ($(Embedded),NO)
USERDEFS = kextmanager.defs
endif

Extra_CC_Flags = -Wall -Werror
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)/$(Install_Dir)/reboot $(DSTROOT)$(Install_Dir)/halt
	$(LN) -fs reboot.8 $(DSTROOT)/usr/share/man/man8/halt.8
