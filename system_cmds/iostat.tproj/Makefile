Project = iostat
Install_Dir = /usr/sbin

CFILES = iostat.c
MANPAGES = iostat.8

Extra_CC_Flags =
Extra_LD_Flags = -dead_strip

Extra_Frameworks = -framework CoreFoundation -framework IOKit

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
