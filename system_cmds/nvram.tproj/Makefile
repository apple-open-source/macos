Project = nvram
Install_Dir = /usr/sbin

CFILES = nvram.c
MANPAGES = nvram.8

Extra_CC_Flags = -mdynamic-no-pic -Wall -Werror
Extra_LD_Flags = -dead_strip

Extra_Frameworks = -framework CoreFoundation -framework IOKit

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
