Project = dynamic_pager
Install_Dir = /sbin

CFILES = dynamic_pager.c
SERVERDEFS = default_pager_alerts.defs \
	backing_store_triggers.defs
USERDEFS = backing_store_alerts.defs
MANPAGES = dynamic_pager.8
LAUNCHD_PLISTS = com.apple.dynamic_pager.plist

Extra_CC_Flags = -Wall -Werror
Extra_LD_Flags = -dead_strip

Extra_CC_Flags += -DNO_DIRECT_RPC

Extra_Frameworks += -framework CoreFoundation -framework SystemConfiguration
Extra_MIG_Flags = -R -untyped -DNO_DIRECT_RPC

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

ifneq ($(SDKROOT),)
Extra_MIG_Flags = -arch $(firstword $(RC_ARCHS))
endif

after_install:
ifeq "$(Embedded)" "YES"
	/usr/libexec/PlistBuddy -x \
	-c "Add :LaunchOnlyOnce bool true" \
	"$(DSTROOT)/System/Library/LaunchDaemons/com.apple.dynamic_pager.plist"
endif
	plutil -convert binary1 "$(DSTROOT)/System/Library/LaunchDaemons/com.apple.dynamic_pager.plist"
