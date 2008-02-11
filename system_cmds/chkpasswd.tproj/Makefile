Project = chkpasswd
Install_Dir = /usr/libexec

HFILES = stringops.h
CFILES = ds_passwd.c nis_passwd.c file_passwd.c passwd.c\
         stringops.c
MANPAGES = chkpasswd.8

Extra_Frameworks = -framework DirectoryService

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
