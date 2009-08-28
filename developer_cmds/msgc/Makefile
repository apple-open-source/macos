Project = msgc
Install_Dir = /usr/bin

HFILES = defs.h msgdb.h
OTHERLINKED = msgparse.y msgscan.l
CFILES = msgdb.c msgmain.c
MANPAGES = msgc.1

OTHERLINKEDOFILES = msgparse.o msgscan.o

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
