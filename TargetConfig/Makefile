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
Install_Dir = /usr/bin

ifeq ($(RC_ProjectName),TargetConfig)
CFILES = tconf.c utils.c
MANPAGES = tconf.1
endif

Extra_CC_Flags = -Wall -Werror
Extra_Frameworks = -framework CoreFoundation

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

BIN = bin
DATDIR = /usr/share/TargetConfigs
FEATURES = Features
FEATURESBINDIR = $(DATDIR)/$(BIN)
FEATURESDIR = $(DATDIR)/$(FEATURES)
FEATURESCRIPTS = feature_scripts
FEATURESCRIPTSDIR = $(DATDIR)/$(FEATURESCRIPTS)
INCDIR = /usr/include

ifndef RC_TARGET_CONFIG
RC_TARGET_CONFIG=MacOSX
export RC_TARGET_CONFIG
endif

ifeq ($(RC_ProjectName),TargetConfig_sdk)
SUBDIR=$(SDKROOT)
HEADERSPHASE=installhdrs_sdk
endif
ifeq ($(RC_ProjectName),TargetConfig_host)
HEADERSPHASE=installhdrs_sdk
endif
ifeq ($(RC_ProjectName),TargetConfig_headers)
HEADERSPHASE=installhdrs_sdk
endif
ifeq ($(RC_ProjectName),TargetConfig)
HEADERSPHASE=installhdrs_default
endif
#ifeq ($(HEADERSPHASE),)
#$(error unknown project name: $(RC_ProjectName))
#endif

installhdrs_sdk:
	$(INSTALL_DIRECTORY) "$(DSTROOT)/$(SUBDIR)/$(DATDIR)"
	$(LN) -fs "$(RC_TARGET_CONFIG)".plist "$(DSTROOT)/$(SUBDIR)/$(DATDIR)"/Default.plist
	tconf --export-header > "$(OBJROOT)"/TargetConfig.h
	$(INSTALL_DIRECTORY) "$(DSTROOT)/$(SUBDIR)/$(INCDIR)"
	$(INSTALL_FILE) "$(OBJROOT)"/TargetConfig.h "$(DSTROOT)/$(SUBDIR)/$(INCDIR)"

installhdrs_default:
	$(INSTALL_DIRECTORY) "$(DSTROOT)/$(DATDIR)"
	$(INSTALL_FILE) *.plist "$(DSTROOT)/$(DATDIR)"

installhdrs:: $(HEADERSPHASE)

after_install: installhdrs
ifeq ($(RC_ProjectName),TargetConfig)
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(FEATURESBINDIR)
	@set -x && \
	cd $(SRCROOT)/$(BIN) && \
	for file in *; do \
	    if [ -L $$file ]; then \
		$(CP) $$file $(DSTROOT)/$(FEATURESBINDIR); \
		$(CHOWN) -h $(Install_Program_User):$(Install_Program_Group) $(DSTROOT)/$(FEATURESBINDIR)/$$file; \
	    else \
		$(INSTALL_SCRIPT) $$file $(DSTROOT)/$(FEATURESBINDIR); \
	    fi || exit 1; \
	done
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(FEATURESCRIPTSDIR)
	@set -x && \
	cd $(SRCROOT)/$(FEATURESCRIPTS) && \
	$(INSTALL_FILE) 0* $(DSTROOT)/$(FEATURESCRIPTSDIR) && \
	for file in `ls | grep -v '^0'`; do \
	    if [ -L $$file ]; then \
		$(CP) $$file $(DSTROOT)/$(FEATURESCRIPTSDIR); \
	    elif [ -f $$file ]; then \
		$(INSTALL_SCRIPT) $$file $(DSTROOT)/$(FEATURESCRIPTSDIR); \
	    fi || exit 1; \
	done
endif
