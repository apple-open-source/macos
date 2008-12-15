Project = chmod
Install_Dir = /bin

CFILES = chmod.c chmod_acl.c
MANPAGES = chmod.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
