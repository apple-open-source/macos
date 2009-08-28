Project = localedef
Install_Dir = /usr/bin

MANPAGES = localedef.1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_PROGRAM) localedef $(DSTROOT)$(Install_Dir)
