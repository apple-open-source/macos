Project = unc
Install_Dir = /usr/lib/system
ProductType = dylib
BuildProfile = YES
BuildDebug = YES

CFILES = UNCUserNotification.c $(OBJROOT)/$(Project)/_version.c
Extra_LD_Flags = -nostdlib -umbrella System -L/usr/lib/system -lsystem_c -lsystem_kernel -llaunch -ldyld

Install_Private_Headers = UNCUserNotification.h

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

$(OBJROOT)/$(Project)/_version.c:
	/Developer/Makefiles/bin/version.pl Libunc > $@
