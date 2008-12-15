# Building TargetConfig has some special requirements because the tconf(1)
# portion is a "host" tool, while the property lists and header are mastered
# into the target SDK.
#
# TargetConfig - installhdrs:
#	install MacOSX.plist
#	install Default.plist -> $(RC_TARGET_CONFIG).plist
# TargetConfig - install:
#	build tconf(1) using "host" SDK
# TargetConfig_sdk, TargetConfig_host - installhdrs, install:
#	install TargetConfig.h using `tconf --export-header`
#	relies on the Default.plist in $(SDKROOT), or alternatively the
#	$(RC_TARGET_CONFIG).plist 

Project = TargetConfig
ProductName = tconf
Install_Dir = /usr/local/bin

ifeq ($(RC_ProjectName),TargetConfig)
CFILES = tconf.c utils.c
MANPAGES = tconf.1
endif

Extra_CC_Flags = -g -Wall -Werror
Extra_Frameworks = -framework CoreFoundation

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

DATDIR = /usr/local/share/TargetConfigs
INCDIR = /usr/local/include

ifndef RC_TARGET_CONFIG
RC_TARGET_CONFIG=MacOSX
export RC_TARGET_CONFIG
endif

installhdrs::
ifeq ($(findstring TargetConfig_,$(RC_ProjectName)),)
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(DATDIR)
	$(INSTALL_FILE) *.plist $(DSTROOT)/$(DATDIR)
else
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(DATDIR)
	$(LN) -fs $(RC_TARGET_CONFIG).plist $(DSTROOT)/$(DATDIR)/Default.plist
	tconf --export-header > $(OBJROOT)/TargetConfig.h
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(INCDIR)
	$(INSTALL_FILE) $(OBJROOT)/TargetConfig.h $(DSTROOT)/$(INCDIR)
endif

after_install: installhdrs
