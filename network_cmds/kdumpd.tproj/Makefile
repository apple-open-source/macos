Project = kdumpd
Install_Dir = /usr/libexec

CFILES = kdumpd.c kdumpsubs.c
HFILES = kdump.h kdumpsubs.h
MANPAGES = kdumpd.8
LAUNCHD_PLISTS = com.apple.kdumpd.plist

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-I.

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
