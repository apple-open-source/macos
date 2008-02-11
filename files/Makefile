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
SRC_HIERARCHY=hierarchy

Product=$(shell tconf --product)
ifeq "$(Product)" "AppleTV"
	SRC_HIERARCHY=hierarchy hierarchy.AppleTV
endif
ifeq "$(Product)" "iPhone"
	SRC_HIERARCHY=hierarchy hierarchy.iPhone
endif

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
ifeq "$(shell tconf --test TARGET_OS_EMBEDDED)" "YES"
	$(_v) $(LN) -fs private/var/tmp "$(Destination)/tmp"
else
	$(_v) $(LN) -fs private/tmp "$(Destination)/tmp"
endif
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/tmp"
	$(_v) $(LN) -fs private/var "$(Destination)/var"
	$(_v) $(CHOWN) -h root:wheel "$(Destination)/var"
	$(_v) $(COMPRESSMANPAGES) /usr/share/man
