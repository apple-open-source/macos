Project = menuc
Install_Dir = /usr/bin

HFILES = defs.h mdb.h
OTHERLINKED = parse.y scan.l
CFILES = avl.c main.c mdb.c util.c y.c
MANPAGES = menuc.1

OTHERLINKEDOFILES = parse.o scan.o

Extra_CC_Flags = -mdynamic-no-pic -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
