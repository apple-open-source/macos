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
# installhdrs.make
#
# Rules for installing public and private header files of a project.  This
# target is invoked automatically from the install target.
#
# PUBLIC TARGETS
#    installhdrs: copies public and private header files to their
#        final destination
#
# IMPORTED VARIABLES
#    BEFORE_INSTALLHDRS: targets to build before installing headers for a subproject
#    AFTER_INSTALLHDRS: targets to build after installing headers for a subproject
#
# EXPORTED VARIABLES
#    PUBLIC_HDR_INSTALLDIR:  where to install public headers.  Don't forget
#        to prefix this with DSTROOT when you use it.
#    PRIVATE_HDR_INSTALLDIR:  where to install private headers.  Don't forget
#	 to prefix this with DSTROOT when you use it.
#

.PHONY: installhdrs local-installhdrs recursive-installhdrs
.PHONY: install-public-headers install-private-headers announce-installhdrs

#
# Variable definitions
#

ACTUAL_INSTALLHDRS = install-public-headers install-private-headers

#
# First we do local installation, then we recurse
#

ifneq "YES" "$(SUPPRESS_BUILD)"
installhdrs: local-installhdrs recursive-installhdrs
recursive-installhdrs: $(ALL_SUBPROJECTS:%=installhdrs@%) local-installhdrs
$(ALL_SUBPROJECTS:%=installhdrs@%): local-installhdrs
endif

#
# Local installation
#

local-installhdrs: announce-installhdrs $(BEFORE_INSTALLHDRS) $(ACTUAL_INSTALLHDRS) $(AFTER_INSTALLHDRS)
$(AFTER_INSTALLHDRS): announce-installhdrs $(BEFORE_INSTALLHDRS) $(ACTUAL_INSTALLHDRS)
$(ACTUAL_INSTALLHDRS): announce-installhdrs $(BEFORE_INSTALLHDRS)
$(BEFORE_INSTALLHDRS): announce-installhdrs

#
# Before we do anything we must announce our intentions
# We don't announce our intentions if we were hooked in from the
# postinstall recursion, since the postinstall has already been announced
#

announce-installhdrs:
ifeq "installhdrs" "$(RECURSING_ON_TARGET)"
ifndef RECURSING
	$(SILENT) $(ECHO) Installing header files...
else
	$(SILENT) $(ECHO) $(RECURSIVE_ELLIPSIS)in $(NAME)
endif
endif

#
# Ensure that important directories exist
#

installhdrs-directories: $(SFILE_DIR)

#
# If there are any public headers we must copy them to the destination directory
#

ifneq "" "$(PUBLIC_HEADERS)$(OTHER_PUBLIC_HEADERS)"
install-public-headers: installhdrs-directories $(DSTROOT)$(PUBLIC_HDR_INSTALLDIR)$(PUBLIC_HEADER_DIR_SUFFIX) $(PUBLIC_HEADERS) $(OTHER_PUBLIC_HEADERS)
ifneq "$(PUBLIC_HDR_INSTALLDIR)$(PUBLIC_HEADER_DIR_SUFFIX)" ""
	$(SILENT) $(FASTCP) $(PUBLIC_HEADERS) $(OTHER_PUBLIC_HEADERS) $(DSTROOT)$(PUBLIC_HDR_INSTALLDIR)$(PUBLIC_HEADER_DIR_SUFFIX)
else
	$(SILENT) $(ECHO) Must set a public header directory to install public headers. ;  exit 1
endif
endif

#
# The same goes for private headers
#

ifneq "" "$(PRIVATE_HEADERS)$(OTHER_PRIVATE_HEADERS)"
install-private-headers: installhdrs-directories $(DSTROOT)$(PRIVATE_HDR_INSTALLDIR)$(PUBLIC_HEADER_DIR_SUFFIX) $(PRIVATE_HEADERS) $(OTHER_PRIVATE_HEADERS)
ifneq "$(PRIVATE_HDR_INSTALLDIR)$(PUBLIC_HEADER_DIR_SUFFIX)" ""
	$(SILENT) $(FASTCP) $(PRIVATE_HEADERS) $(OTHER_PRIVATE_HEADERS) $(DSTROOT)$(PRIVATE_HDR_INSTALLDIR)$(PUBLIC_HEADER_DIR_SUFFIX)
else
	$(SILENT) $(ECHO) Must set a private header directory to install private headers. ;  exit 1
endif
endif

#
# Rules for creating directories
#

$(DSTROOT)$(PUBLIC_HDR_INSTALLDIR)$(PUBLIC_HEADER_DIR_SUFFIX) $(DSTROOT)$(PRIVATE_HDR_INSTALLDIR)$(PRIVATE_HEADER_DIR_SUFFIX):
	$(MKDIRS) $@
