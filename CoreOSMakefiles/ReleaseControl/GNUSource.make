##
# Makefile for Apple Release Control (GNU source projects)
#
# Wilfredo Sanchez | wsanchez@apple.com
# Copyright (c) 1997-1999 Apple Computer, Inc.
#
# @APPLE_LICENSE_HEADER_START@
# 
# Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
# Reserved.  This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.1 (the "License").  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
# 
# The Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
# Set these variables as needed, then include this file, then:
#
#  Install_Prefix        [ $(USRDIR)                              ]
#  Install_Man           [ $(MANDIR)                              ]
#  Install_Info          [ $(SHAREDIR)/info                       ]
#  Install_HTML          [ <depends>                              ]
#  Install_Source        [ $(NSSOURCEDIR)/Commands/$(ProjectName) ]
#  Configure             [ $(Sources)/configure                   ]
#  Extra_Configure_Flags
#  Extra_Install_Flags 
#  Passed_Targets        [ check                                  ]
#
# Additional variables inherited from ReleaseControl/Common.make
##

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

Passed_Targets += check

include $(CoreOSMakefiles)/ReleaseControl/Common.make

##
# My variables
##

Sources     = $(SRCROOT)/$(Project)
ConfigStamp = $(BuildDirectory)/configure-stamp

ifndef Install_Prefix
Install_Prefix = $(USRDIR)
endif
ifndef Install_Man
Install_Man = $(MANDIR)
endif
ifndef Install_Info
Install_Info = $(SHAREDIR)/info
endif
ifndef Install_HTML
ifeq "$(UserType)" "Developer"
Install_HTML = $(SYSTEM_DEVELOPER_TOOLS_DOC_DIR)/$(ProjectName)
else
Install_HTML = $(NSDOCUMENTATIONDIR)/$(ToolType)/$(ProjectName)
endif
endif
ifndef Install_Source
Install_Source = $(NSSOURCEDIR)/$(ToolType)/$(ProjectName)
endif

RC_Install_Prefix = $(DSTROOT)$(Install_Prefix)
RC_Install_Man    = $(DSTROOT)$(Install_Man)
RC_Install_Info   = $(DSTROOT)$(Install_Info)
RC_Install_HTML   = $(DSTROOT)$(Install_HTML)
ifneq ($(Install_Source),)
RC_Install_Source = $(DSTROOT)$(Install_Source)
endif

ifndef Configure
Configure = $(Sources)/configure
endif

Environment += TEXI2HTML="$(TEXI2HTML) -subdir ."

# 4158518: gcc-4.0 no longer allows multiple archs with -M*, so projects
# that have dependency tracking on by default, fail.  So we set
# --disable-dependency-tracking to turn off dependency tracking.
Configure_Flags = --prefix="$(Install_Prefix)"	\
		  --mandir="$(Install_Man)"	\
		  --infodir="$(Install_Info)"	\
		  --disable-dependency-tracking \
		  $(Extra_Configure_Flags)

Install_Flags = prefix="$(RC_Install_Prefix)"	\
		mandir="$(RC_Install_Man)"	\
	       infodir="$(RC_Install_Info)"	\
	       htmldir="$(RC_Install_HTML)"	\
	               $(Extra_Install_Flags)

Install_Target = install-strip

##
# Targets
##

.PHONY: configure almostclean

install:: build
ifneq ($(GnuNoInstall),YES)
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) $(Install_Target)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) -depth -exec $(RMDIR) "{}" \;
	$(_v) $(FIND) $(SYMROOT) $(Find_Cruft) -depth -exec $(RMDIR) "{}" \;
ifneq ($(GnuNoChown),YES)
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT) $(SYMROOT)
endif
endif
ifdef GnuAfterInstall
	$(_v) $(MAKE) $(GnuAfterInstall)
endif
	$(_v) $(MAKE) compress_man_pages

build:: configure
ifneq ($(GnuNoBuild),YES)
	@echo "Building $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment)
endif

configure:: lazy_install_source $(ConfigStamp)

reconfigure::
	$(_v) $(RM) $(ConfigStamp)
	$(_v) $(MAKE) configure

$(ConfigStamp):
ifneq ($(GnuNoConfigure),YES)
	@echo "Configuring $(Project)..."
	$(_v) $(MKDIR) $(BuildDirectory)
# Disable LD_TRACE_FILE during configure
	$(_v) cd $(BuildDirectory) && $(Environment) LD_TRACE_FILE=/dev/null $(Configure) $(Configure_Flags)
endif
	$(_v) touch $@

almostclean::
ifneq ($(GnuNoClean),YES)
	@echo "Cleaning $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory) clean
endif
