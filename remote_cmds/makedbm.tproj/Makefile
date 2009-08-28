Project = makedbm
Install_Dir = /usr/sbin

HFILES = _db.h ypdb.h ypdef.h
CFILES = db.c makedbm.c ypdb.c
MANPAGES = makedbm.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
