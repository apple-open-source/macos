Project = dirhelper
Install_Dir = /usr/libexec

CFILES = dirhelper.c
SERVERDEFS = /usr/local/include/dirhelper.defs
MANPAGES = dirhelper.8
LAUNCHD_PLISTS = com.apple.bsd.dirhelper.plist

Extra_CC_Flags = -mdynamic-no-pic
Extra_CC_Flags += -D__MigTypeCheck=1
Extra_LD_Flags = -dead_strip
Extra_LD_Flags += -lbsm

#CFLAGS = -g -mdynamic-no-pic -Os -Wall -Wextra -Wshadow -Wmissing-prototypes -Wmissing-declarations -Werror -D__MigTypeCheck=1 $(RC_CFLAGS) -I $(OBJDIR)

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
