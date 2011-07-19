Project = notifyd
Install_Dir = /usr/sbin

HFILES = notifyd.h table.h service.h timer.h pathwatch.h
CFILES = notifyd.c timer.c pathwatch.c service.c notify_proc.c
USERDEFS = $(SDKROOT)/usr/local/include/notify_ipc.defs
SERVERDEFS = $(SDKROOT)/usr/local/include/notify_ipc.defs
MANPAGES = notifyd.8
LAUNCHD_PLISTS = com.apple.notifyd.plist

Extra_CC_Flags = -Wall -I. -I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -dead_strip -lbsm

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

# Select correct config file for platform
Product=$(shell tconf --product)
SRC_NOTIFY_CONF=notify.conf
ifeq "$(Product)" "iPhone"
SRC_NOTIFY_CONF=notify.conf.iPhone
endif

ifneq ($(SDKROOT),)
Extra_MIG_Flags = -arch $(firstword $(RC_ARCHS))
endif

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/etc
	$(INSTALL_FILE) $(SRC_NOTIFY_CONF) $(DSTROOT)/private/etc/notify.conf
	$(CHMOD) 0644 $(DSTROOT)/private/etc/notify.conf
	codesign -s- $(DSTROOT)/usr/sbin/notifyd
	plutil -convert binary1 $(DSTROOT)/System/Library/LaunchDaemons/com.apple.notifyd.plist
