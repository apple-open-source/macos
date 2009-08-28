# Copyright (c) 2007-2008 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
#
# @APPLE_LICENSE_HEADER_END@

#
# Makefile for smb-odauth-helper
#

Project := smb-odauth-helper

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

VERBOSE := YES
ifeq ($(SRCROOT),.)
	COPY_SOURCES = NO
endif

ifeq ($(GnuNoStrip),YES)
STRIP_X	:= true
else
STRIP_X	:= strip -x
endif

include $(CoreOSMakefiles)/ReleaseControl/Common.make

# Reset the various roots for in-place builds. Common.make sets
# these to subdirectories of /tmp but does not bother to actually
# create the subdirectories. This makes in-place builds fail because
# no-one has got around to doing a mkdir on $(SYMROOT). buildit
# sets these paths in the environment, which overrides the local
# values, making us all OK.

SRCROOT = .
OBJROOT = .
SYMROOT = .
DSTROOT = .

# make.common contains the project-wide GCC options and other useful settings.
# when doing a stand-alone buldit from the current directory, it's not copied
# to $(SRCROOT), so we have to live without it.
-include ../../make.common

SMB_LIBEXEC ?= $(LIBEXECDIR)/samba

DEBUG ?= NO
TESTLEAKS ?= NO

# Turn off optimisation for debug builds. There's so much inlining going
# on that it's just too hard to step through otherwise.
ifeq ($(DEBUG),YES)
CC_Optimize := -O0
endif

Extra_CC_Flags += $(RC_CFLAGS)

LIBS := -framework Security -framework CoreFoundation

TARGETS := $(OBJROOT)/smb-odauth-helper

all: $(TARGETS)
build:: $(TARGETS)

$(OBJROOT)/smb-odauth-helper: authenticator.o
	@echo Linking $@ ...
	$(_v) $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Install tools to $DSTROOT and copy the symbolicated versions to
# $SYMROOT. This is already done in the Samba project as part of the
# GnuAfterInstall rule. We only need to do this when doing a buildit
# build of the tools sub-project.
install:: $(TARGETS)
	$(INSTALL) -d -m 755 $(DSTROOT)$(SMB_LIBEXEC)
	$(INSTALL) -c -m 755 -o root -g wheel \
		$(OBJROOT)/smb-odauth-helper \
		$(DSTROOT)$(SMB_LIBEXEC)
ifndef $(NoSymRootCopy)
	$(INSTALL) -d -m 755 $(SYMROOT)$(SMB_LIBEXEC)
	$(INSTALL) -c -m 755 -o root -g wheel \
		$(OBJROOT)/smb-odauth-helper \
		$(SYMROOT)$(SMB_LIBEXEC)
	dsymutil $(SYMROOT)$(SMB_LIBEXEC)/smb-odauth-helper
endif

clean::
	-$(_v) rm -rf $(KEYCHAIN_OBJECTS) *.dSYM
	-$(_v) rm -f $(TARGETS)

