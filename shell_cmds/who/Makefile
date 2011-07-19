Project = who
Install_Dir = /usr/bin

CFILES = who.c utmpentry.c
MANPAGES = who.1

Extra_CC_Flags = -Wall -Werror
Extra_CC_Flags += -DSUPPORT_UTMPX -D_UTMPX_COMPAT
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
