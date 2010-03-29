Project = notifyd
Install_Dir = /usr/sbin

HFILES = daemon.h file_watcher.h service.h\
         table.h w_event.h watcher.h
CFILES = daemon.c file_watcher.c notify_proc.c service.c w_event.c\
         watcher.c
USERDEFS = $(SDKROOT)/usr/local/include/notify_ipc.defs
SERVERDEFS = $(SDKROOT)/usr/local/include/notify_ipc.defs
MANPAGES = notifyd.8
LAUNCHD_PLISTS = com.apple.notifyd.plist

Extra_CC_Flags = -Wall -mdynamic-no-pic -I.
Extra_LD_Flags = -dead_strip -lbsm

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

ifneq ($(SDKROOT),)
Extra_MIG_Flags = -arch $(firstword $(RC_ARCHS))
endif

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/etc
	$(INSTALL_FILE) notify.conf $(DSTROOT)/private/etc
	$(CHMOD) 0644 $(DSTROOT)/private/etc/notify.conf
	codesign -s- $(DSTROOT)/usr/sbin/notifyd
	plutil -convert binary1 $(DSTROOT)/System/Library/LaunchDaemons/com.apple.notifyd.plist
