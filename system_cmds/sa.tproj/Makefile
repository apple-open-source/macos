Project = sa
Install_Dir = /usr/sbin

CFILES = main.c pdb.c usrdb.c db.c
MANPAGES = sa.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_CC_Flags += -DAHZV1=64
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
