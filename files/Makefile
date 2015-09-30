##
# Mac OS X file system hierarchy
# Copyright 1999-2007 Apple Inc.
##

export Project = files

Destination = $(DSTROOT)

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

##
# Read the hierarchy file and create the directories.
# If the corresponding directory exists in the SRCROOT
# and contains a Makefile, execute that makefile.
##
ifdef XBS_PROJECT_COMPILATION_PLATFORM
CONTENT_PLATFORM=$(XBS_PROJECT_COMPILATION_PLATFORM)
else
CONTENT_PLATFORM=osx
# <rdar://problem/19355870> buildit doesn't set XBS_PROJECT_CONTENT_PLATFORMS nor XBS_PROJECT_COMPILATION_PLATFORM
ifeq "$(RC_TARGET_CONFIG)" "iPhone"
ifeq "$(RC_ProjectName)" "files_Sim"
CONTENT_PLATFORM=ios_sim
else
CONTENT_PLATFORM=ios
endif
endif
endif

ifeq "$(CONTENT_PLATFORM)" "atv"
CONTENT_PLATFORM=ios
endif
ifeq "$(CONTENT_PLATFORM)" "atv_sim"
CONTENT_PLATFORM=ios_sim
endif
ifeq "$(CONTENT_PLATFORM)" "watch"
CONTENT_PLATFORM=ios
endif
ifeq "$(CONTENT_PLATFORM)" "watch_sim"
CONTENT_PLATFORM=ios_sim
endif

ifeq "$(CONTENT_PLATFORM)" "ios_sim"
SRC_HIERARCHY=hierarchy
else
SRC_HIERARCHY=hierarchy hierarchy.not_sim hierarchy.$(CONTENT_PLATFORM)
endif

install::
	@echo "Installing for $(CONTENT_PLATFORM)"
	$(_v) install -d -m 1775 -o root -g admin "$(Destination)"
	$(_v) set -o pipefail && cat $(SRC_HIERARCHY) | \
	awk -F '\t'  ' \
	{	print sprintf("install -d -m %s -o %s -g %s \"$(Destination)/%s\";", $$1, $$2, $$3, $$4); \
		print sprintf("[ ! -f \"%s/Makefile\" ] || make -C \"%s\" CONTENT_PLATFORM=\"$(CONTENT_PLATFORM)\" Destination=\"$(Destination)/%s\" $@ ;", $$4, $$4, $$4); \
	}' | sh -x -e

install::
	$(_v) $(LN) -fs private/etc "$(Destination)/etc"
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/etc"
ifeq "$(CONTENT_PLATFORM)" "ios"
	$(_v) $(LN) -fs private/var/tmp "$(Destination)/tmp"
	$(_v) $(LN) -fs ../private/var/logs "$(Destination)/Library/Logs"
	$(_v) $(LN) -fs "../../private/var/Managed Preferences/mobile" "$(Destination)/Library/Managed Preferences/mobile"
	$(_v) $(LN) -fs ../private/var/preferences "$(Destination)/Library/Preferences"
	$(_v) $(LN) -fs ../private/var/Keychains "$(Destination)/Library/Keychains"
	$(_v) $(LN) -fs ../private/var/MobileDevice "$(Destination)/Library/MobileDevice"
endif
ifeq "$(CONTENT_PLATFORM)" "osx"
	$(_v) $(INSTALL_FILE) -m 0644 -o root -g admin -c /dev/null "$(Destination)/.com.apple.timemachine.donotpresent"
	#$(_v) $(INSTALL_FILE) -m 0644 -o root -g admin -c /dev/null "$(Destination)/.metadata_never_index"
	$(_v) $(LN) -fs private/tmp "$(Destination)/tmp"
	$(_v) $(INSTALL) -m 0664 -o root -g admin -c /dev/null "$(Destination)/.DS_Store"
	$(_v) $(INSTALL) -m 0664 -o root -g admin -c /dev/null "$(Destination)/Applications/.DS_Store"
	$(_v) $(INSTALL) -m 0664 -o root -g admin -c /dev/null "$(Destination)/Applications/Utilities/.DS_Store"
	# rdar://problem/9596025
	$(_v) $(LN) -fs ../../Applications/Motion.app/Contents/Frameworks/AEProfiling.framework "$(Destination)/Library/Frameworks"
	$(_v) $(LN) -fs ../../Applications/Motion.app/Contents/Frameworks/AERegistration.framework "$(Destination)/Library/Frameworks"
	$(_v) $(LN) -fs ../../Applications/Motion.app/Contents/Frameworks/AudioMixEngine.framework "$(Destination)/Library/Frameworks"
endif
ifneq "$(CONTENT_PLATFORM)" "ios_sim"
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/tmp"
	$(_v) $(CHMOD) -h 0755 "$(Destination)/tmp"
	$(_v) $(LN) -fs private/var "$(Destination)/var"
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/var"

	$(TOUCH) "$(Destination)/.file"
	$(_v) $(CHOWN) root:nogroup "$(Destination)/.file"
	$(_v) $(CHMOD) 0 "$(Destination)/.file"

	# $(SYMROOT)/bsd.sb is created by usr/share/sandbox/Makefile
	# rdar://problem/8207011
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)/usr/local/share/sandbox/profiles/embedded/imports"
	$(_v) $(INSTALL_FILE) $(SYMROOT)/bsd.sb "$(Destination)/usr/local/share/sandbox/profiles/embedded/imports/bsd.sb"
ifeq "$(CONTENT_PLATFORM)" "osx"
	# rdar://problem/11108634
	$(_v) $(INSTALL_FILE) $(SYMROOT)/bsd.sb "$(Destination)/System/Library/Sandbox/Profiles/bsd.sb"
endif
endif
