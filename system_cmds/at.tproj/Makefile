Project = at
Install_Dir = /usr/bin

HFILES = at.h panic.h parsetime.h pathnames.h privs.h perm.h
CFILES = at.c panic.c parsetime.c perm.c
MANPAGES = at.1

PROJECT_HEADERS = privs.h pathnames.h

Extra_CC_Flags = -D__FBSDID=__RCSID -DDAEMON_UID=1 -DDAEMON_GID=1 \
	-DDEFAULT_AT_QUEUE='a' -DDEFAULT_BATCH_QUEUE='b' \
	-DPERM_PATH=\"/usr/lib/cron/\" \
	-I/System/Library/Frameworks/System.framework/PrivateHeaders

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555

after_install:
	$(LN) -f $(DSTROOT)/$(Install_Dir)/at $(DSTROOT)$(Install_Dir)/atq
	$(LN) -f $(DSTROOT)/$(Install_Dir)/at $(DSTROOT)$(Install_Dir)/atrm
	$(LN) -f $(DSTROOT)/$(Install_Dir)/at $(DSTROOT)$(Install_Dir)/batch
	install -o daemon -d "$(DSTROOT)/private/var/at/spool"
	touch "$(DSTROOT)/private/var/at/at.deny"
	$(MKDIR) "$(DSTROOT)/usr/lib"
	$(LN) -sf ../../var/at "$(DSTROOT)/usr/lib/cron"
	$(LN) -sf at.1 "$(DSTROOT)/usr/share/man/man1/atrm.1"
	$(LN) -sf at.1 "$(DSTROOT)/usr/share/man/man1/atq.1"
	$(LN) -sf at.1 "$(DSTROOT)/usr/share/man/man1/batch.1"
