Project = cron
UserType = Developer
ToolType = Agregate
BSD_After_Install = dsyms install_launchd_plist install-plist

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

CC_Debug = -g -Wall -W -no-cpp-precomp -mdynamic-no-pic
Extra_CC_Flags += -I/System/Library/Frameworks/System.framework/PrivateHeaders

dsyms:
	cp "$(OBJROOT)/cron" "$(SYMROOT)/cron"
	dsymutil --out="$(SYMROOT)/cron.dSYM" "$(SYMROOT)/cron"
	cp "$(OBJROOT)/crontab" "$(SYMROOT)/crontab"
	dsymutil --out="$(SYMROOT)/crontab.dSYM" "$(SYMROOT)/crontab"

install_launchd_plist::
	mkdir -p $(DSTROOT)/System/Library/LaunchDaemons
	install -m 644 com.vix.cron.plist $(DSTROOT)/System/Library/LaunchDaemons
	install -o daemon -d "$(DSTROOT)/private/var/at"
	touch "$(DSTROOT)/private/var/at/cron.deny"
	mkdir -p "$(DSTROOT)/private/var/at/tabs"
	mkdir -p "$(DSTROOT)/private/var/at/tmp"
	chmod 700 "$(DSTROOT)/private/var/at/tabs" "$(DSTROOT)/private/var/at/tmp"

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/LICENSE $(OSL)/$(Project).txt
