Project = rlogin
Install_Dir = /usr/bin

HFILES = krb.h
CFILES = des_rw.c kcmd.c krcmd.c rlogin.c
MANPAGES = rlogin.1

Extra_CC_Flags += -DCRYPT

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
