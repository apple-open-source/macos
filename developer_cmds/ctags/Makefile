Project = ctags
Install_Dir = /usr/bin

HFILES = ctags.h
CFILES = C.c ctags.c fortran.c lisp.c print.c tree.c yacc.c
MANPAGES = ctags.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
