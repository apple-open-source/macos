Project = ypinit
Install_Dir = /usr/sbin

MANPAGES = ypinit.8

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

VARYP=$(DSTROOT)/private/var/yp

after_install:
	$(INSTALL_DIRECTORY) $(VARYP)
	$(INSTALL_FILE) Makefile.yp $(VARYP)
	$(INSTALL_FILE) Makefile.main $(VARYP)

	$(INSTALL_DIRECTORY) $(DSTROOT)/$(Install_Dir)
	$(INSTALL_SCRIPT) ypinit.sh $(DSTROOT)/$(Install_Dir)/ypinit
