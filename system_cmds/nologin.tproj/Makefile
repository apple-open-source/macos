Project = nologin
Install_Dir = /sbin

MANPAGES = nologin.8

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_SCRIPT) nologin.sh $(DSTROOT)/$(Install_Dir)/nologin
