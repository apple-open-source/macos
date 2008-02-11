Project = passwd
Install_Dir = /usr/bin

HFILES = stringops.h
CFILES = passwd.c file_passwd.c stringops.c
MANPAGES = passwd.1

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

ifeq ($(Embedded),NO)
CFILES += nis_passwd.c od_passwd.c
Extra_Frameworks = -F/System/Library/PrivateFrameworks -framework OpenDirectory -framework CoreFoundation
Extra_CC_Flags = -F/System/Library/PrivateFrameworks
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
