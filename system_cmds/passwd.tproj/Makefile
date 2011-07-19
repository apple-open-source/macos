Project = passwd
Install_Dir = /usr/bin

CFILES = passwd.c file_passwd.c
MANPAGES = passwd.1

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

Extra_CC_Flags = -Wall -Werror
Extra_LD_Flags = -dead_strip

ifeq ($(Embedded),NO)
CFILES += nis_passwd.c od_passwd.c pam_passwd.c
Extra_Frameworks = -framework OpenDirectory -framework CoreFoundation -lpam
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 0555

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/private/etc/pam.d
	$(INSTALL_FILE) passwd.pam "$(DSTROOT)"/private/etc/pam.d/passwd
