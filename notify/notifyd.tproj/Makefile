Project = notifyd
Install_Dir = /usr/sbin

HFILES = common.h daemon.h file_watcher.h notify_ipc_types.h service.h\
         table.h w_event.h watcher.h
CFILES = daemon.c file_watcher.c notify_proc.c service.c w_event.c\
         watcher.c
USERDEFS = notify_ipc.defs
SERVERDEFS = notify_ipc.defs
MANPAGES = notifyd.8
LAUNCHD_PLISTS = com.apple.notifyd.plist

Extra_CC_Flags = -I.

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/etc
	$(INSTALL_FILE) notify.conf $(DSTROOT)/private/etc
	$(CHMOD) 0644 $(DSTROOT)/private/etc/notify.conf
