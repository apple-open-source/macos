Project = newgrp
Install_Dir = /usr/bin

CFILES = newgrp.c
MANPAGES = newgrp.1

Extra_CC_Flags = -Wall -Werror
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
