Project = TargetConfig
ProductName = tconf
Install_Dir = /usr/local/bin

CFILES = tconf.c utils.c

Extra_CC_Flags = -g -Wall -Werror
Extra_Frameworks = -framework CoreFoundation

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

DATDIR = /usr/local/share/TargetConfigs
INCDIR = /usr/local/include

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(DATDIR)
	$(INSTALL_FILE) *.plist $(DSTROOT)/$(DATDIR)
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(INCDIR)
	$(DSTROOT)/$(Install_Dir)/$(ProductName) --export-header > \
		$(DSTROOT)/$(INCDIR)/TargetConfig.h
