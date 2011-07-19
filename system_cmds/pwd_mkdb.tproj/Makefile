Project = pwd_mkdb
Install_Dir = /usr/sbin

HFILES = pw_scan.h
CFILES = pwd_mkdb.c pw_scan.c
OTHERSRCS = Makefile.preamble Makefile Makefile.postamble pwd_mkdb.8
MANPAGES = pwd_mkdb.8

Extra_CC_Flags = -Wall -Werror
Extra_LD_Flags = -dead_strip

Extra_CC_Flags += -D_PW_NAME_LEN=MAXLOGNAME -D_PW_YPTOKEN=\"__YP!\"

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
