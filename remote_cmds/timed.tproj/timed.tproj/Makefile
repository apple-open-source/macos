Project = timed
Install_Dir = /usr/sbin

HFILES = extern.h globals.h pathnames.h
CFILES = acksend.c byteorder.c candidate.c cksum.c correct.c master.c\
         measure.c networkdelta.c readmsg.c slave.c timed.c
MANPAGES = timed.8

Extra_CC_Flags = -Wall -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
