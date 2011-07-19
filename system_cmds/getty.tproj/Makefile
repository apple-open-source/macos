Project = getty
Install_Dir = /usr/libexec

HFILES = extern.h gettytab.h pathnames.h
CFILES = chat.c init.c main.c subr.c
MANPAGES = getty.8 gettytab.5 ttys.5

Embedded = $(shell tconf --test TARGET_OS_EMBEDDED)
ifeq ($(Embedded),YES)
LAUNCHD_PLISTS = com.apple.getty.plist
endif

Extra_CC_Flags = -Wall -Werror
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

ifeq ($(Embedded),YES)
after_install:
	plutil -convert binary1 "$(DSTROOT)/System/Library/LaunchDaemons/$(LAUNCHD_PLISTS)"
endif
