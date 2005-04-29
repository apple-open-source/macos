Project = cron
UserType = Developer
ToolType = Agregate
BSD_After_Install = install_launchd_plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

CC_Debug = -Wall -W -no-cpp-precomp -mdynamic-no-pic
Extra_CC_Flags += -I/System/Library/Frameworks/System.framework/PrivateHeaders

install_launchd_plist::
	mkdir -p $(DSTROOT)/private/var/cron/tabs
	mkdir -p $(DSTROOT)/System/Library/LaunchDaemons
	mkdir -p $(DSTROOT)/private/etc
	install -m 644 com.vix.cron.plist $(DSTROOT)/System/Library/LaunchDaemons
	install -m 644 crontab.default $(DSTROOT)/private/etc/crontab
