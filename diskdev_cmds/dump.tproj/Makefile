Project = dump
ProductType = tool
Install_Dir = /sbin

CFILES = itime.c main.c optr.c tape.c traverse.c unctime.c
MANPAGES = dump.8 rdump.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1 \
	-DRDUMP -I../disklib
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f "$(DSTROOT)$(Install_Dir)/dump" "$(DSTROOT)$(Install_Dir)/rdump"
