Project = update
Install_Dir = /usr/sbin

CFILES = update.c
MANPAGES = update.8
LAUNCHD_PLISTS = com.apple.update.plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

SANDBOX = "$(DSTROOT)/usr/share/sandbox"

after_install:
	$(INSTALL_DIRECTORY) $(SANDBOX)
	$(INSTALL_FILE) update.sb $(SANDBOX)
