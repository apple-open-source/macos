Project = shar
Install_Dir = /usr/bin

MANPAGES = shar.1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_SCRIPT) shar.sh $(DSTROOT)/$(Install_Dir)/shar
