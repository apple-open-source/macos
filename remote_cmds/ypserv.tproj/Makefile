Project = ypserv
Install_Dir = /usr/libexec

HFILES = acl.h yp.h ypv1.h ypdb.h yplog.h ypdef.h
CFILES = acl.c ypdb.c yplog.c ypserv.c ypserv_db.c ypserv_proc.c\
         ypserv_xdr.c ypserv_xdr_v1.c
MANPAGES = securenet.5 ypserv.acl.5 ypserv.8
LAUNCHD_PLISTS = com.apple.nis.ypserv.plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
