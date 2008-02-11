Project = rlogind
Install_Dir = /usr/libexec

HFILES = pathnames.h
CFILES = rlogind.c
MANPAGES = rlogind.8
LAUNCHD_PLISTS = login.plist

Extra_CC_Flags += -DCRYPT

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
