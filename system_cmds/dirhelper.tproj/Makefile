Project = dirhelper
Install_Dir = /usr/libexec

CFILES = dirhelper.c
SERVERDEFS = /usr/local/include/dirhelper.defs
MANPAGES = dirhelper.8
LAUNCHD_PLISTS = com.apple.bsd.dirhelper.plist

Extra_CC_Flags = -D__MigTypeCheck=1
#CFLAGS = -g -mdynamic-no-pic -Os -Wall -Wextra -Wshadow -Wmissing-prototypes -Wmissing-declarations -Werror -D__MigTypeCheck=1 $(RC_CFLAGS) -I $(OBJDIR)
Extra_LD_Flags = -lbsm

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
