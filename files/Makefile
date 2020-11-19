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
ifeq "$(subst Bridge_,,$(RC_ProjectName))" "files_Sim"
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

ifeq "$(CONTENT_PLATFORM)" "osx"
# This really should go to /AppleInternal, but can't because of rdar://9402760.
DATA_SYMLINK_PREFIX=/DataVolumeSymlinks
# No leading / since this is relative
DATA_SYMLINK_DEST=System/Volumes/Data
endif

install::

	# The hierarchy files REQUIRE that their columns be TAB-SEPARATED. Using
	# spaces will result in that line being SILENTLY IGNORED. This should be
	# fixed at some point.
	@echo "Installing for $(CONTENT_PLATFORM)"
	$(_v) install -d -m 1775 -o root -g admin "$(Destination)"
	$(_v) set -o pipefail && cat $(SRC_HIERARCHY) | \
	awk -F '\t'  ' \
	{	if (NF!=4) { print sprintf("echo \"\033[0;31mMake sure to use tabs instead of spaces for: %s\033[0m\"; exit 1;", $$0); exit 1} \
		print sprintf("install -d -m %s -o %s -g %s \"$(Destination)/%s\";", $$1, $$2, $$3, $$4); \
		print sprintf("[ ! -f \"%s/Makefile\" ] || make -C \"%s\" CONTENT_PLATFORM=\"$(CONTENT_PLATFORM)\" Destination=\"$(Destination)/%s\" $@ ;", $$4, $$4, $$4); \
	}' | sh -x -e

ifeq "$(CONTENT_PLATFORM)" "osx"
	# Create symlinks at the firmlink paths, but prepend a fixed prefix
	# which will be removed at Mastering time to avoid any overlapping
	# files issues.
	$(_v) set -o pipefail && cat usr/share/firmlinks | awk -F '\t' '{ \
		if (NF!=2) { print sprintf("echo \"\033[0;31mMake sure to use tabs instead of spaces for firmlinks\033[0m\"; exit 1;"); exit 1}; \
		print "mkdir -p", "$(Destination)/\$$(dirname $(DATA_SYMLINK_PREFIX)"$$1")"; \
		relpath=$$2; sub("[^/]*$$", "", relpath); gsub("[^/]+", "..", relpath); \
		print "ln -s", relpath "$(DATA_SYMLINK_DEST)/" $$2, "$(Destination)/" "$(DATA_SYMLINK_PREFIX)" $$1}' | sh -x -e
endif

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

	# Set up symlink for custom volume icons on the system volume.
	#
	# <rdar://problem/52896013>
	$(_v) $(LN) -fs System/Volumes/Data/.VolumeIcon.icns "$(Destination)/.VolumeIcon.icns"
	$(_v) $(INSTALL) -m 0664 -o root -g admin -c /dev/null "$(Destination)/.DS_Store"
	$(_v) $(INSTALL) -m 0664 -o root -g admin -c /dev/null "$(Destination)/Applications/.DS_Store"
	$(_v) $(INSTALL) -m 0664 -o root -g admin -c /dev/null "$(Destination)/Applications/Utilities/.DS_Store"

	# <rdar://problem/63655752>
	$(_v) $(LN) -fs SystemVersion.plist "$(Destination)/System/Library/CoreServices/.SystemVersionPlatform.plist"
endif
ifneq "$(CONTENT_PLATFORM)" "ios_sim"
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/tmp"
	$(_v) $(CHMOD) -h 0755 "$(Destination)/tmp"
	$(_v) $(LN) -fs private/var "$(Destination)/var"
ifeq "$(CONTENT_PLATFORM)" "osx"
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/var"
else
	$(_v) $(CHOWN) -h root:admin "$(Destination)/var"
endif

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
