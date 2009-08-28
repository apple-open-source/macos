Project = ps
Install_Dir = /bin

CFILES = fmt.c keyword.c nlist.c print.c ps.c tasks.c
MANPAGES = ps.1
Extra_CC_Flags = -mdynamic-no-pic -D__FBSDID=__RCSID -Wall -Werror -Wno-\#warnings
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
