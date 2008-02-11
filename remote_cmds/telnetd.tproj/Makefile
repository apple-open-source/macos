Project = telnetd
Install_Dir = /usr/libexec

HFILES = defs.h ext.h pathnames.h telnetd.h
CFILES = global.c slc.c state.c sys_term.c telnetd.c\
         termstat.c utility.c
MANPAGES = telnetd.8
LAUNCHD_PLISTS = telnet.plist

Extra_CC_Flags = -DNO_UTMP -DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOLD_ENVIRON -DENV_HACK # -DAUTHENTICATION -DENCRYPTION
Extra_LD_Flags = -lcurses -ltelnet

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Product=$(shell tconf --product)
Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

after_install:
ifeq "$(Embedded)" "YES"
	/usr/libexec/PlistBuddy -x \
		-c "Delete :Disabled" \
		-c "Delete :SessionCreate" \
		-c "Add :Sockets:Listeners:SockFamily string IPv4" \
		"$(DSTROOT)/System/Library/LaunchDaemons/telnet.plist"
ifeq "$(Product)" "iPhone"
	/usr/libexec/PlistBuddy -x \
		-c "Set :Sockets:Listeners:Bonjour false" \
		-c "Add :Sockets:Listeners:SockNodeName string localhost" \
		"$(DSTROOT)/System/Library/LaunchDaemons/telnet.plist"
endif
endif
