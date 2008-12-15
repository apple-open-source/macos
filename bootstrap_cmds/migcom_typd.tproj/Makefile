Project= migcom_typd
ProductType = tool
Install_Dir = /usr/lib

LFILES = lexxer.l
YFILES = parser.y

CFILES = error.c global.c header.c migcom.c routine.c server.c\
         statement.c string.c type.c user.c utils.c

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-I.
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
