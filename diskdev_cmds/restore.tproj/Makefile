Project = restore
ProductType = tool
Install_Dir = /sbin

HFILES = extern.h pathnames.h restore.h
CFILES = dirs.c interactive.c main.c restore.c symtab.c tape.c\
         utilities.c
MANPAGES = restore.8 rrestore.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1 \
	-DRRESTORE
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Group = tty

after_install:
	$(LN) -f "$(DSTROOT)$(Install_Dir)"/restore "$(DSTROOT)$(Install_Dir)"/rrestore
