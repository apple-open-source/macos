##
# Files
# Wilfredo Sanchez | wsanchez@apple.com
# Copyright 1999 Apple Computer, Inc.
##

# Project info
export Project = files

Destination = $(DSTROOT)

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = Library Network Users private usr

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_DIRECTORY) -m 1777 "$(Destination)/cores"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)/dev"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)/System"
	$(_v) $(INSTALL_FILE) -c .hidden "$(Destination)/.hidden"
	$(_v) $(LN) -fs private/etc      "$(Destination)/etc"
	$(_v) $(LN) -fs mach_kernel      "$(Destination)/mach"
	$(_v) $(LN) -fs private/tmp      "$(Destination)/tmp"
	$(_v) $(LN) -fs private/var      "$(Destination)/var"
