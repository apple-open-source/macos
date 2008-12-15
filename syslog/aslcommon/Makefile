Project = aslcommon
ProductType = staticlib
Install_Dir = /scratch

CFILES = asl_memory.c asl_mini_memory.c
SERVERDEFS = asl_ipc.defs
USERDEFS = asl_ipc.defs

#PROJECT_HEADERS = asl_ipc.defs asl_memory.h asl_mini_memory.h asl_ipc.h

Extra_CC_Flags = -Wall -D__MigTypeCheck=1 -I.

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
