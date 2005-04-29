##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# The contents of this file constitute Original Code as defined in and
# are subject to the Apple Public Source License Version 1.1 (the
# "License").  You may not use this file except in compliance with the
# License.  Please obtain a copy of the License at
# http://www.apple.com/publicsource and read it before using this file.
# 
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
#
# installsrc.make
#
# Rules for installing public and private header files of a project.  This
# target is invoked automatically from the install target.
#
# PUBLIC TARGETS
#    installsrc: copies source code from the current directory to a
#	specified directory.
#
# IMPORTED VARIABLES
#    BEFORE_INSTALLSRC: targets to build before installing source for a subproject
#    AFTER_INSTALLSRC: targets to build after installing source for a subproject
#
# EXPORTED VARIABLES
#    SRCROOT:  base directory in which to place the new source files
#    SRCPATH:  relative path from SRCROOT to present subdirectory
#

.PHONY: installsrc local-installsrc recursive-installsrc
.PHONY: install-source-files

#
# Variable definitions
#

ACTUAL_INSTALLSRC = install-source-files

#
# First we do local installation, then we recurse.  Note that,
# unlike other recursive builds, source installation is never
# suppressed.
#

ifeq "$(SRCROOT)" "$(shell pwd)"
installsrc:
	$(SILENT) $(ECHO) Error:  must set SRCROOT before building installsrc
	exit 1
else
installsrc: local-installsrc recursive-installsrc
recursive-installsrc: $(ALL_SUBPROJECTS:%=installsrc@%) local-installsrc
$(ALL_SUBPROJECTS:%=installsrc@%): local-installsrc
endif

#
# Local installation
#

local-installsrc: announce-installsrc $(BEFORE_INSTALLSRC) $(ACTUAL_INSTALLSRC) $(AFTER_INSTALLSRC)
$(AFTER_INSTALLSRC): announce-installsrc $(BEFORE_INSTALLSRC) $(ACTUAL_INSTALLSRC)
$(ACTUAL_INSTALLSRC): announce-installsrc $(BEFORE_INSTALLSRC)
$(BEFORE_INSTALLSRC): announce-installsrc

#
# Before we do anything we must announce our intentions
# We don't announce our intentions if we were hooked in from the
# postinstall recursion, since the postinstall has already been announced
#

announce-installsrc:
ifndef RECURSING
	$(SILENT) $(ECHO) Installing source files...
else
	$(SILENT) $(ECHO) $(RECURSIVE_ELLIPSIS)in $(NAME)
endif

#
# Copy source files
#

IMPLICIT_SOURCE_FILES += Makefile

install-source-files: $(SRCFILES) $(IMPLICIT_SOURCE_FILES)
	$(RM) -rf $(SRCROOT)$(SRCPATH)
	$(MKDIRS) $(SRCROOT)$(SRCPATH)
	$(TAR) Pcf - $(SRCFILES) | ( cd $(SRCROOT)$(SRCPATH) && $(TAR) Pxf - )
	$(SILENT) for i in $(IMPLICIT_SOURCE_FILES) none ; do \
            if [ -r $$i -a ! -r $(SRCROOT)$(SRCPATH)/$$i ] ; then \
                supportfiles="$$supportfiles $$i" ; \
            fi ; \
        done ; \
        if [ -n "$$supportfiles" ] ; then \
	   $(ECHO) "$(TAR) Pcf - $$supportfiles | ( cd $(SRCROOT)$(SRCPATH) && $(TAR) Pxf - )"  ; \
	   $(TAR) Pcf - $$supportfiles | ( cd $(SRCROOT)$(SRCPATH) && $(TAR) Pxf - ) ; \
        fi

