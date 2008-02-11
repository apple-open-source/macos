Project = login
Install_Dir = /usr/bin


HFILES = pathnames.h
CFILES = klogin.c login.c
MANPAGES = login.1

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

ifeq "$(Embedded)" "NO"
Extra_LD_Flags = -lbsm -lpam -lpam_misc
Extra_CC_Flags = -DUSE_PAM -DUSE_BSM -no-cpp-precomp
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
