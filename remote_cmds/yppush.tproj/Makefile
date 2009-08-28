Project = yppush
Install_Dir = /usr/sbin

HFILES = yppush.h ypdb.h yplib_host.h ypdef.h
CFILES = ypdb.c yplib_host.c yppush.c yppush_err.c yppush_proc.c\
         yppush_svc.c yppush_xdr.c
MANPAGES = yppush.8

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_LD_Flags = -dead_strip -pie

Extra_CC_Flags += -DSYSLOG

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
