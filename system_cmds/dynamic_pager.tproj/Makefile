Project = dynamic_pager
Install_Dir = /sbin

CFILES = dynamic_pager.c
SERVERDEFS = default_pager_alerts.defs \
	backing_store_triggers.defs
USERDEFS = backing_store_alerts.defs
MANPAGES = dynamic_pager.8
LAUNCHD_PLISTS = com.apple.dynamic_pager.plist

Extra_CC_Flags = -no-cpp-precomp -DNO_DIRECT_RPC

Extra_LD_Flags = -framework CoreFoundation -framework IOKit
Extra_MIG_Flags = -no-cpp-precomp -R -untyped -DNO_DIRECT_RPC

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)
        
after_install:
ifeq "$(Embedded)" "YES"
	/usr/libexec/PlistBuddy -x \
	-c "Add :Disabled bool true" \
	"$(DSTROOT)/System/Library/LaunchDaemons/com.apple.dynamic_pager.plist"
endif
