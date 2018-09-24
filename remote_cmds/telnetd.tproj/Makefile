Project = telnetd
ifeq "$(RC_TARGET_CONFIG)" "iPhone"
Install_Dir = /usr/libexec
else
Install_Dir = /usr/local/libexec
endif

HFILES = defs.h ext.h pathnames.h telnetd.h
CFILES = global.c slc.c state.c sys_term.c telnetd.c\
         termstat.c utility.c
ifeq "$(RC_TARGET_CONFIG)" "iPhone"
LAUNCHD_PLISTS = telnet.plist
endif

Extra_CC_Flags = -Wall -Werror -Wno-string-plus-int -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

Extra_CC_Flags += -DNO_UTMP -DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO \
		-DDIAGNOSTICS -DOLD_ENVIRON -DENV_HACK -DINET6 \
		# -DAUTHENTICATION -DENCRYPTION
Extra_LD_Libraries = -lcurses -ltelnet

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
ifeq "$(RC_TARGET_CONFIG)" "iPhone"
	/usr/libexec/PlistBuddy -x \
		-c "Delete :Disabled" \
		-c "Add :PosixSpawnType string Interactive" \
		-c "Delete :SessionCreate" \
		-c "Set :Sockets:Listeners:Bonjour false" \
		-c "Add :Sockets:Listeners:SockFamily string IPv4" \
		-c "Add :Sockets:Listeners:SockNodeName string localhost" \
		"$(DSTROOT)/System/Library/LaunchDaemons/telnet.plist"
ifeq "$(RC_PLATFORM_NAME)" "BridgeOS"
	/usr/libexec/PlistBuddy -x \
		-c "Delete :Sockets:Listeners:SockNodeName" \
		-c "Delete :Sockets:Listeners:SockFamily" \
		"$(DSTROOT)/System/Library/LaunchDaemons/telnet.plist"
endif
	plutil -convert binary1 "$(DSTROOT)/System/Library/LaunchDaemons/$(LAUNCHD_PLISTS)"
endif
	/usr/bin/codesign --force --sign - --entitlements entitlements.plist $(DSTROOT)$(Install_Dir)/$(Project)

# Install a special launchd plist for the DebugDiskImage (38885624)
ifeq "$(RC_TARGET_CONFIG)" "iPhone"
	plutil -replace ProgramArguments -json '["/var/personalized_debug/usr/libexec/telnetd","-p","/var/personalized_debug/usr/bin/login"]' -o "$(DSTROOT)/System/Library/LaunchDaemons/telnet.debug.plist" "$(DSTROOT)/System/Library/LaunchDaemons/telnet.plist"
	plutil -replace Label -string com.apple.telnetd.debug "$(DSTROOT)/System/Library/LaunchDaemons/telnet.debug.plist"
endif
