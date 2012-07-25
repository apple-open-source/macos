##
# Mac OS X file system hierarchy
# Copyright 1999-2007 Apple Inc.
##

export Project = files

Destination = $(DSTROOT)

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

##
# Read the hierarchy file and create the directories.
# If the corresponding directory exists in the SRCROOT
# and contains a Makefile, execute that makefile.
##
Product=$(shell tconf --product)
SRC_HIERARCHY=hierarchy hierarchy.$(Product)

install::
	@echo "Installing for $(Product)"
	$(_v) install -d -m 1775 -o root -g admin "$(Destination)"
	$(_v) cat $(SRC_HIERARCHY) | \
	awk -F '\t'  ' \
	{	print sprintf("install -d -m %s -o %s -g %s \"$(Destination)/%s\";", $$1, $$2, $$3, $$4); \
		print sprintf("[ ! -f \"%s/Makefile\" ] || make -C \"%s\" Destination=\"$(Destination)/%s\" $@ ;", $$4, $$4, $$4); \
	}' | sh -x -e

install::
	$(_v) $(LN) -fs private/etc "$(Destination)/etc"
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/etc"
ifeq "$(Embedded)" "YES"
	$(_v) $(LN) -fs private/var/tmp "$(Destination)/tmp"
	$(_v) $(LN) -fs ../private/var/logs "$(Destination)/Library/Logs"
ifeq "$(Product)" "iPhone"
	$(_v) $(LN) -fs "../../private/var/Managed Preferences/mobile" "$(Destination)/Library/Managed Preferences/mobile"
endif
	$(_v) $(LN) -fs ../private/var/preferences "$(Destination)/Library/Preferences"
	$(_v) $(LN) -fs ../private/var/Keychains "$(Destination)/Library/Keychains"
	$(_v) $(LN) -fs ../private/var/MobileDevice "$(Destination)/Library/MobileDevice"
else
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
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/tmp"
	$(_v) $(CHMOD) -h 0755 "$(Destination)/tmp"
	$(_v) $(LN) -fs private/var "$(Destination)/var"
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/var"
	$(TOUCH) "$(Destination)/.file"
	$(_v) $(CHOWN) root:nogroup "$(Destination)/.file"
	$(_v) $(CHMOD) 0 "$(Destination)/.file"
