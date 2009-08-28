Project = tftp

HFILES = extern.h tftpsubs.h
CFILES = main.c tftp.c tftpsubs.c
MANPAGES = tftp.1

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie
Extra_LD_Flags += -ledit

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
