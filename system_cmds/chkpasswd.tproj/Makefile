Project = chkpasswd
Install_Dir = /usr/libexec

CFILES = od_passwd.c nis_passwd.c file_passwd.c pam_passwd.c passwd.c\
	stringops.c
MANPAGES = chkpasswd.8

Extra_CC_Flags = -Wall -Werror
Extra_LD_Flags = -dead_strip

Extra_Frameworks = -framework CoreFoundation -framework OpenDirectory -lpam

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 00555

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)"/private/etc/pam.d
	$(INSTALL_FILE) chkpasswd.pam "$(DSTROOT)"/private/etc/pam.d/chkpasswd
