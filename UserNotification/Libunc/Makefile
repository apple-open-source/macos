Project = unc
Install_Dir = /usr/local/lib/system
ProductType = staticlib
BuildProfile = YES
BuildDebug = YES

CFILES = UNCUserNotification.c $(OBJROOT)/$(Project)/_version.c

Install_Private_Headers = UNCUserNotification.h

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

$(OBJROOT)/$(Project)/_version.c:
	/Developer/Makefiles/bin/version.pl Libunc > $@
