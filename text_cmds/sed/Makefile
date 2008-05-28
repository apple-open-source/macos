Project = sed
Install_Dir = /usr/bin

CFILES = compile.c main.c misc.c process.c
MANPAGES = sed.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID \
	-D__DARWIN_UNIX03=1
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
