Project = md5
Install_Dir = /sbin

CFILES = md5.c
MANPAGES = md5.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip \
	-lmd -lcrypto

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
