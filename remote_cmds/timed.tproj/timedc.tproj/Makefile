Project = timedc
Install_Dir = /usr/sbin

HFILES = extern.h timedc.h
CFILES = cmds.c cmdtab.c timedc.c \
	../timed.tproj/byteorder.c \
	../timed.tproj/measure.c \
	../timed.tproj/cksum.c
MANPAGES = timedc.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

