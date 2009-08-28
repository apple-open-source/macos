Project = login
Install_Dir = /usr/bin


HFILES = pathnames.h
CFILES = login.c
MANPAGES = login.1

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

Extra_CC_Flags += -D__FBSDID=__RCSID

ifneq ($(Embedded),YES)
CFILES += login_audit.c
Extra_CC_Flags += -DUSE_PAM -DUSE_BSM_AUDIT
Extra_LD_Flags += -lbsm -lpam
else
Extra_CC_Flags += -Wno-deprecated-declarations
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/private/etc/pam.d
	$(INSTALL_FILE) login.pam "$(DSTROOT)"/private/etc/pam.d/login
	sed -e '/pam_launchd.so/d' < login.pam > "$(OBJROOT)"/login.term
	$(INSTALL_FILE) "$(OBJROOT)"/login.term "$(DSTROOT)"/private/etc/pam.d/login.term
