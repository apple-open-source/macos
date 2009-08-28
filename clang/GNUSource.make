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
#  Passed_Targets        [                                        ]
#
# Additional variables inherited from ReleaseControl/Common.make
##

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

include $(CoreOSMakefiles)/ReleaseControl/Common.make

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

Environment = CC="$(CC) -arch $$arch" CXX="$(CXX) -arch $$arch" ONLY_MAN_DOCS=1

CC_Archs      = # set by CC
# FIXME: Common.make shouldn't be setting this in the first place.
Extra_CC_Flags =

# -arch arguments are different than configure arguments. We need to
# translate them.

TRANSLATE_ARCH=$(SED) -e s/ppc/powerpc/ -e s/i386/i686/
# Could use config.guess here, if we had a copy available.
BUILD=`$(ARCH) | $(TRANSLATE_ARCH)`-apple-darwin

Configure_Flags = --prefix="$(Install_Prefix)"	\
		  $(Extra_Configure_Flags)      \
		  `case $$arch in x86_64) echo --build=x86_64-apple-darwin$$(uname -r | sed 's/\..*//');; esac`

Install_Flags = DESTDIR=$(BuildDirectory)/install-$$arch \
		$(Extra_Install_Flags)

Install_Target = install-strip

##
# Targets
##

.PHONY: configure almostclean

SYSCTL := $(shell sysctl -n hw.activecpu)

install:: build
ifneq ($(GnuNoInstall),YES)
	$(_v) for arch in $(RC_ARCHS) ; do \
		echo "Installing $(Project) for $$arch..." && \
		$(MKDIR) $(BuildDirectory)/install-$$arch && \
		umask $(Install_Mask) && \
		$(MAKE) -C $(BuildDirectory)/$$arch $(Environment) \
			$(Install_Flags) $(Install_Target) || exit 1 ; \
	done
	$(_v) rm -rf $(DSTROOT)
	$(_v) mkdir -p $(DSTROOT)
	./merge-lipo `for arch in $(RC_ARCHS) ; do echo $(BuildDirectory)/install-$$arch ; done` $(DSTROOT)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v) $(FIND) $(SYMROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v) $(FIND) $(DSTROOT) -perm -0111 ! -name ccc -type f -print | $(XARGS) -n 1 -P $(SYSCTL) dsymutil
	$(_v) cd $(DSTROOT) && find . -path \*.dSYM/\* -print | cpio -pdml $(SYMROOT)
	$(_v) find $(DSTROOT) -perm -0111 ! -name ccc -type f -print | xargs -P $(SYSCTL) strip
	$(_v) find $(DSTROOT) -name \*.dSYM -print | xargs rm -r
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

build:: configure
ifneq ($(GnuNoBuild),YES)
	$(_v) for arch in $(RC_ARCHS) ; do \
		echo "Building $(Project) for $$arch..." && \
		$(MAKE) -j$(SYSCTL) -C $(BuildDirectory)/$$arch $(Environment) $(Build_Target) || exit 1; \
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
