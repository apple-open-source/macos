Project = w
Install_Dir = /usr/bin

HFILES = extern.h
CFILES = fmt.c proc_compare.c pr_time.c w.c
MANPAGES = uptime.1 w.1

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

Extra_CC_Flags = -Wall \
	-no-cpp-precomp -Wno-error -DHAVE_KVM=0 -DHAVE_UTMPX=1
Extra_LD_Flags = -dead_strip
ifeq ($(Embedded),NO)
Extra_LD_Flags += -lresolv
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) $(DSTROOT)/$(Install_Dir)/w $(DSTROOT)/$(Install_Dir)/uptime
