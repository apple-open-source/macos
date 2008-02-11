Project= shutdown
Install_Dir = /sbin

HFILES = pathnames.h
CFILES = shutdown.c
USERDEFS = kextmanager.defs
MANPAGES = shutdown.8

Extra_Frameworks = -framework IOKit
Extra_LD_Flags = -lbsm

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
