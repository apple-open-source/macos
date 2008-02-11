Project = reboot
Install_Dir = /sbin

CFILES = reboot.c
MANPAGES = reboot.8

Embedded = $(shell tconf --test TARGET_OS_EMBEDDED)
ifeq ($(Embedded),NO)
USERDEFS = kextmanager.defs
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) $(DSTROOT)/$(Install_Dir)/reboot $(DSTROOT)$(Install_Dir)/halt
	$(LN) -fs reboot.8 $(DSTROOT)/usr/share/man/man8/halt.8
