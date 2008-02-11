Project = pagesize
Install_Dir = /usr/bin

MANPAGES = pagesize.1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_SCRIPT) pagesize.sh $(DSTROOT)/$(Install_Dir)/pagesize
