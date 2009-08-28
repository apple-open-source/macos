Project = rexecd
Install_Dir = /usr/libexec

CFILES = rexecd.c
MANPAGES = rexecd.8
LAUNCHD_PLISTS = exec.plist

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
