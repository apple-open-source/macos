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

Extra_CC_Flags_saved:=$(Extra_CC_Flags)
# Common.make adds multiple -arch options to Extra_CC_Flags. Firstly, it should
# NOT dink with this. Secondly, this breaks everything.
include $(CoreOSMakefiles)/ReleaseControl/Common.make
Extra_CC_Flags=$(Extra_CC_Flags_saved)

##
# My variables
##

Sources     = $(SRCROOT)/$(Project)
ConfigStamp = $(BuildDirectory)/configure-stamp

Workaround_3678855 = /BogusHTMLInstallationDir

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
Install_HTML = $(Workaround_3678855)
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
Environment += CC="$(CC) -arch $$arch" CXX="$(CXX) -arch $$arch" 
Environment += AS="$(AS) -arch $$arch" LD="$(LD) -arch $$arch"
Environment += NM="nm -arch $$arch"
Environment += AR=$(AR) STRIP=$(STRIP) RANLIB=ranlib

CC_Archs      = # set by CC

# -arch arguments are different than configure arguments. We need to
# translate them.
XLATE_ARCH := $(SED) -e s/ppc/powerpc/ -e s/i386/i686/

# Could use config.guess here, if we had a copy available.
BUILDHOST := `$(ARCH) | $(XLATE_ARCH)`-apple-darwin

# 4158518: gcc-4.0 no longer allows multiple archs with -M*, so projects
# that have dependency tracking on by default, fail.  So we set
# --disable-dependency-tracking to turn off dependency tracking.
Configure_Flags = --prefix="$(Install_Prefix)"	\
		  --mandir="$(Install_Man)"	\
		  --infodir="$(Install_Info)"	\
		  --disable-dependency-tracking \
		  --build=$(BUILDHOST) \
		  --host=`echo $$arch | $(XLATE_ARCH)`-apple-darwin \
		  $(Extra_Configure_Flags)

Install_Flags = DESTDIR=$(BuildDirectory)/install-$$arch \
	               $(Extra_Install_Flags)

Install_Target = install-strip

##
# Targets
##

.PHONY: configure almostclean

install:: build
ifneq ($(GnuNoInstall),YES)
	$(_v) for arch in $(RC_ARCHS) ; do \
		echo "Installing $(Project) for $$arch..." && \
		$(MKDIR) $(BuildDirectory)/install-$$arch && \
		umask $(Install_Mask) && \
		$(MAKE) -C $(BuildDirectory)/$$arch $(Environment) \
			$(Install_Flags) $(Install_Target) || exit 1 ; \
	done
	./scripts/merge-lipo `for arch in $(RC_ARCHS) ; do echo $(BuildDirectory)/install-$$arch ; done` $(DSTROOT)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v) $(FIND) $(SYMROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
ifneq ($(GnuNoChown),YES)
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT) $(SYMROOT)
endif
endif
ifdef GnuAfterInstall
	$(_v) $(MAKE) $(GnuAfterInstall)
endif
	$(_v) if [ -d "$(DSTROOT)$(Workaround_3678855)" ]; then \
		$(INSTALL_DIRECTORY) "$(DSTROOT)$(SYSTEM_DEVELOPER_TOOLS_DOC_DIR)"; \
		$(MV) "$(DSTROOT)$(Workaround_3678855)" \
			"$(DSTROOT)$(SYSTEM_DEVELOPER_TOOLS_DOC_DIR)/$(ProjectName)"; \
	fi
	$(_v) $(MAKE) compress_man_pages

build:: configure
ifneq ($(GnuNoBuild),YES)
	$(_v) for arch in $(RC_ARCHS) ; do \
		echo "Building $(Project) for $$arch..." && \
		$(MAKE) -C $(BuildDirectory)/$$arch $(Environment) || exit 1; \
	done
endif

configure:: lazy_install_source $(ConfigStamp)

reconfigure::
	$(_v) $(RM) $(ConfigStamp)
	$(_v) $(MAKE) configure

$(ConfigStamp):
ifneq ($(GnuNoConfigure),YES)
	$(_v) $(MKDIR) $(BuildDirectory)
	$(_v) for arch in $(RC_ARCHS) ; do \
		echo "Configuring $(Project) for $$arch..." && \
		$(MKDIR) $(BuildDirectory)/$$arch && \
		cd $(BuildDirectory)/$$arch && \
		$(Environment) $(Configure) $(Configure_Flags) || exit 1 ; \
	done
endif
	$(_v) touch $@

almostclean::
ifneq ($(GnuNoClean),YES)
	@echo "Cleaning $(Project)..."
	$(_v) for arch in $(RC_ARCHS) ; do \
		$(MAKE) -C $(BuildDirectory)/$$arch clean || exit 1 ; \
	done
endif
