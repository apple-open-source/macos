Project = mklocale
Install_Dir = /usr/bin

YFILES = yacc.y
LFILES = lex.l
MANPAGES = mklocale.1

Extra_CC_Flags = -mdynamic-no-pic
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_CC_Flags += -I.
Extra_CC_Flags += -Wall -Werror
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
