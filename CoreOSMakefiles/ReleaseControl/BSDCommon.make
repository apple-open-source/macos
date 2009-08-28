##
# Makefile for Apple Release Control (Apple projects)
#
# Copyright (c) 2007 Apple Inc.  All rights reserved.
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
##

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

###           ###
### Variables ###
###           ###

Library_Prefix ?= lib
RELEASE_Library_Suffix = $(Library_Suffix)
DEBUG_Library_Suffix = _debug$(Library_Suffix)
PROFILE_Library_Suffix = _profile$(Library_Suffix)

ProductType ?= tool
ifeq ($(ProductType),tool)
    ProductName ?= $(Project)
    Install_Dir ?= /usr/bin
else
 ifeq ($(ProductType),dylib)
    Library_Suffix ?= .dylib
    ProductName ?= $(Library_Prefix)$(Project)$(Library_Suffix)
    ProductNameWithVersion ?= $(Library_Prefix)$(Project)$(if $(Library_Version),.$(Library_Version),)$(Library_Suffix)
    RELEASE_ProductName = $(ProductName)
    RELEASE_ProductNameWithVersion = $(ProductNameWithVersion)
    DEBUG_ProductName = $(ProductName:$(Library_Suffix)=$(DEBUG_Library_Suffix))
    DEBUG_ProductNameWithVersion = $(ProductNameWithVersion:$(Library_Suffix)=$(DEBUG_Library_Suffix))
    PROFILE_ProductName = $(ProductName:$(Library_Suffix)=$(PROFILE_Library_Suffix))
    PROFILE_ProductNameWithVersion = $(ProductNameWithVersion:$(Library_Suffix)=$(PROFILE_Library_Suffix))
    Install_Dir ?= /usr/lib
 else
  ifeq ($(ProductType),staticlib)
    Library_Suffix ?= .a
    ProductName ?= $(Library_Prefix)$(Project)$(Library_Suffix)
    RELEASE_ProductName = $(ProductName)
    DEBUG_ProductName = $(ProductName:$(Library_Suffix)=$(DEBUG_Library_Suffix))
    PROFILE_ProductName = $(ProductName:$(Library_Suffix)=$(PROFILE_Library_Suffix))
    Install_Dir ?= /usr/local/lib
  else
   ifeq ($(ProductType),none)
   else
    $(error Unknown ProductType: $(ProductType))
   endif
  endif
 endif
endif

BUILD_STYLES = RELEASE
ifeq ($(BuildDebug),YES)
BUILD_STYLES += DEBUG
endif
ifeq ($(BuildProfile),YES)
BUILD_STYLES += PROFILE
endif

ALL_FILES = $(CFILES) $(MFILES) $(CXXFILES) $(USERDEFS) $(SERVERDEFS) $(MANPAGES)
ALL_SRCFILES = $(CFILES) $(MFILES) $(CXXFILES)

## MIG ##

MIGFLAGS=$(Extra_MIG_Flags)
ifneq ($(USERDEFS),)
CFILES += $(foreach FILE, $(USERDEFS:.defs=_user.c), $(OBJROOT)/$(Project)/$(notdir $(FILE)))
endif
ifneq ($(SERVERDEFS),)
CFILES += $(foreach FILE, $(SERVERDEFS:.defs=_server.c), $(OBJROOT)/$(Project)/$(notdir $(FILE)))
endif
ifneq ($(USERDEFS) $(SERVERDEFS),)
Extra_CC_Flags += -I$(OBJROOT)/$(Project)
endif

## RPC ##

RPCFLAGS=$(Extra_RPC_Flags)
ifneq ($(RPCFILES),)
CFILES += $(foreach FILE, $(RPCFILES:.x=_xdr.c), $(OBJROOT)/$(Project)/$(notdir $(FILE)))
endif
ifneq ($(RPCSVCFILES),)
CFILES += $(foreach FILE, $(RPCFILES:.x=_svc.c), $(OBJROOT)/$(Project)/$(notdir $(FILE)))
endif

## Lex ##

LFLAGS=$(Extra_L_Flags)
ifneq ($(LFILES),)
CFILES += $(foreach FILE, $(LFILES:.l=.yy.c), $(OBJROOT)/$(Project)/$(notdir $(FILE)))
endif

## Yacc ##

YFLAGS=$(Extra_Y_Flags)
ifneq ($(YFILES),)
CFILES += $(foreach FILE, $(YFILES:.y=.c), $(OBJROOT)/$(Project)/$(notdir $(FILE)))
endif

## SDK Support ##

ifneq ($(SDKROOT),)
Extra_CC_Flags += -isysroot $(SDKROOT)
Extra_LD_Flags += -Wl,-syslibroot,$(SDKROOT)
export MIGCC = $(shell xcrun -find -sdk $(SDKROOT) cc)
endif

## Dylib Support ##

ifeq ($(Library_Compatibility_Version),)
Library_Compatibility_Version = 1
endif
ifeq ($(Library_Current_Version),)
Library_Current_Version = $(if $(RC_ProjectSourceVersion), $(RC_ProjectSourceVersion), 1)
endif

RELEASE_OFILE_SUFFIX=.o
DEBUG_OFILE_SUFFIX=_debug.o
PROFILE_OFILE_SUFFIX=_profile.o

RELEASE_ALL_OFILES = $(foreach OFILE, \
		$(CFILES:.c=.o) \
		$(MFILES:.m=.o) \
		$(CXXFILES:.cc=.o) \
		$(OTHER_OFILES), \
			$(OBJROOT)/$(Project)/$(notdir $(OFILE)))
DEBUG_ALL_OFILES = $(RELEASE_ALL_OFILES:.o=$(DEBUG_OFILE_SUFFIX))
PROFILE_ALL_OFILES = $(RELEASE_ALL_OFILES:.o=$(PROFILE_OFILE_SUFFIX))
ALL_OFILES = $(RELEASE_ALL_OFILES) $(DEBUG_ALL_OFILES) $(PROFILE_ALL_OFILES)

RELEASE_CFLAGS = $(CFLAGS)
DEBUG_CFLAGS = $(CFLAGS) -DDEBUG
PROFILE_CFLAGS = $(CFLAGS) -pg -DPROFILE

RELEASE_LDFLAGS = $(LDFLAGS)
DEBUG_LDFLAGS = $(LDFLAGS)
PROFILE_LDFLAGS = $(LDFLAGS)

###         ###
### Targets ###
###         ###

.PHONY: configure almostclean

Install_Headers_Directory ?= /usr/include
Install_Private_Headers_Directory ?= /usr/local/include

installhdrs:: _installhdrs
ifneq ($(SubProjects),)
	make recurse TARGET=$@ RC_ARCHS="$(RC_ARCHS)"
endif

_installhdrs::
	@true # avoid nothing to be done warning
ifneq ($(Install_Headers),)
	@echo "Installing headers for $(Project)..."
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(Install_Headers_Directory)
	@for HFILE in $(Install_Headers); do \
		CMD="$(INSTALL_FILE) $${HFILE} $(DSTROOT)/$(Install_Headers_Directory)" ; \
                echo $${CMD} ; $${CMD} || exit 1 ; \
	done
endif
ifneq ($(Install_Private_Headers),)
	@echo "Installing private headers for $(Project)..."
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(Install_Private_Headers_Directory)
	@for HFILE in $(Install_Private_Headers); do \
		CMD="$(INSTALL_FILE) $${HFILE} $(DSTROOT)/$(Install_Private_Headers_Directory)" ; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
	done
endif

install:: build _installhdrs
	@echo "====== Installing $(Project) ====="
#ifneq ($(SubProjects),)
#	make recurse TARGET=$@ RC_ARCHS="$(RC_ARCHS)"
#endif
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(Install_Dir)
ifneq ($(strip $(ALL_OFILES)),)
 ifeq ($(ProductType),tool)
	$(INSTALL_PROGRAM) $(SYMROOT)/$(ProductName) $(DSTROOT)/$(Install_Dir)
 else
  ifeq ($(ProductType),dylib)
	@$(foreach STYLE, $(BUILD_STYLES), \
	CMD="$(INSTALL_DYLIB) $(SYMROOT)/$($(STYLE)_ProductNameWithVersion) \
		$(DSTROOT)/$(Install_Dir)" ; \
	echo $${CMD} ; $${CMD} || exit 1 ; \
	CMD="$(STRIP) -S $(DSTROOT)/$(Install_Dir)/$($(STYLE)_ProductNameWithVersion)" ; \
	echo $${CMD} ; $${CMD} || exit 1 ; \
	if [ "$($(STYLE)_ProductName)" != "$($(STYLE)_ProductNameWithVersion)" ]; then \
		CMD="$(LN) -sf $($(STYLE)_ProductNameWithVersion) \
			$(DSTROOT)/$(Install_Dir)/$($(STYLE)_ProductName)" ; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
	fi ; )
  else
   ifeq ($(ProductType),staticlib)
	@$(foreach STYLE, $(BUILD_STYLES), \
		CMD="$(INSTALL_LIBRARY) $(SYMROOT)/$($(STYLE)_ProductName) \
			$(DSTROOT)/$(Install_Dir)" ; \
		echo $${CMD} ; $${CMD} || exit 1 ; )
   endif
  endif
 endif
endif
ifneq ($(MANPAGES),)
	@make install-man-pages
endif
ifneq ($(LAUNCHD_PLISTS),)
	@make install-launchd-plists
endif
	@make after_install
	@$(MAKE) compress_man_pages

migdefs::
	@$(MKDIR) $(OBJROOT)/$(Project)
	@for DEF in $(USERDEFS); do \
		CMD="$(MIG) $(MIGFLAGS) \
		  -user $(OBJROOT)/$(Project)/$$(basename $${DEF/%.defs/_user.c}) \
		  -header $(OBJROOT)/$(Project)/$$(basename $${DEF/%.defs/.h}) \
		  -server /dev/null \
		  -sheader /dev/null \
		  $${DEF}" ; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
	done
	@for DEF in $(SERVERDEFS); do \
		CMD="$(MIG) $(MIGFLAGS) \
		  -user /dev/null \
		  -header /dev/null \
		  -server $(OBJROOT)/$(Project)/$$(basename $${DEF/%.defs/_server.c}) \
		  -sheader $(OBJROOT)/$(Project)/$$(basename $${DEF/%.defs/_server.h}) \
		  $${DEF}" ; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
	done

rpcfiles:
	@$(MKDIR) $(OBJROOT)/$(Project)
	@for FILE in $(RPCFILES); do \
		OUT=`basename $${FILE} .x` ; \
		CMD="$(RPCGEN) $(RPCFLAGS) -h \
			-o $(OBJROOT)/$(Project)/$${OUT}.h $${FILE}"; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
		CMD="$(RPCGEN) $(RPCFLAGS) -c \
			-o $(OBJROOT)/$(Project)/$${OUT}_xdr.c $${FILE}"; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
	done
	@for FILE in $(RPCSVCFILES); do \
		OUT=`basename $${FILE} .x` ; \
		CMD="$(RPCGEN) $(RPCFLAGS) -m \
			-o $(OBJROOT)/$(Project)/$${OUT}_svc.c $${FILE}"; \
		echo $${CMD} ; $${CMD} || exit 1; \
	done

lfiles:
	@$(MKDIR) $(OBJROOT)/$(Project)
	@for FILE in $(LFILES); do \
		OUT=`basename $${FILE} .l` ; \
		CMD="$(LEX) $(LFLAGS) \
			--header-file=$(OBJROOT)/$(Project)/$${OUT}.yy.h \
			-o $(OBJROOT)/$(Project)/$${OUT}.yy.c $${FILE}"; \
		echo $${CMD} ; $${CMD} || exit 1; \
	done

yfiles:
	@$(MKDIR) $(OBJROOT)/$(Project)
	@for FILE in $(YFILES); do \
		OUT=`basename $${FILE} .y` ; \
		CMD="$(YACC) $(YFLAGS) -d \
			-o $(OBJROOT)/$(Project)/$${OUT}.c $${FILE}"; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
	done

build:: migdefs rpcfiles yfiles lfiles $(ALL_FILES)
ifneq ($(SubProjects),)
	make recurse TARGET=install RC_ARCHS="$(RC_ARCHS)"
endif
	@$(MKDIR) $(OBJROOT)/$(Project)
	@$(MKDIR) $(SYMROOT)

	@$(foreach STYLE, $(BUILD_STYLES), \
	echo ===== Building $(ProductName) $(STYLE) ===== ; \
	for CFILE in $(ALL_SRCFILES); do \
		OFILE=$(OBJROOT)/$(Project)/$$(echo $$(basename $${CFILE}) | \
			sed -e 's,\.[^.]*$$,$($(STYLE)_OFILE_SUFFIX),') ; \
		if [ ! "$${OFILE}" -nt "$${CFILE}" ]; then \
		CMD="$(CC) $($(STYLE)_CFLAGS) -c -o $${OFILE} $${CFILE}" ; \
		echo $$CMD ; $$CMD || exit 1 ; \
		fi ; \
	done ; )

ifneq ($(strip $(ALL_OFILES)),)
 ifeq ($(ProductType),tool)
	@echo ===== Linking $(ProductName) RELEASE =====
	$(CC) $(RELEASE_LDFLAGS) -o $(SYMROOT)/$(ProductName) \
		$(RELEASE_ALL_OFILES)
	dsymutil --out $(SYMROOT)/$(ProductName).dSYM \
		$(SYMROOT)/$(ProductName) || true
 else
  ifeq ($(ProductType),dylib)
	@$(foreach STYLE, $(BUILD_STYLES), \
	echo ===== Linking $(ProductName) $(STYLE) ===== ; \
	CMD="$(CC) -dynamiclib $($(STYLE)_LDFLAGS) \
		-dynamic \
		-compatibility_version $(Library_Compatibility_Version) \
		-current_version $(Library_Current_Version) \
		-install_name `echo $(Install_Dir)/$($(STYLE)_ProductNameWithVersion) | sed 's,//,/,g'` \
		-o $(SYMROOT)/$($(STYLE)_ProductNameWithVersion) \
		$($(STYLE)_ALL_OFILES)" ; \
		echo $${CMD} ; $${CMD} || exit 1 ; \
	CMD="dsymutil --out $(SYMROOT)/$($(STYLE)_ProductNameWithVersion).dSYM \
		$(SYMROOT)/$($(STYLE)_ProductNameWithVersion)" ; \
	echo $${CMD} ; $${CMD} ; )
  else
   ifeq ($(ProductType),staticlib)
	@$(foreach STYLE, $(BUILD_STYLES), \
		echo ===== Linking $(ProductName) $(STYLE) ===== ; \
		CMD="$(LIBTOOL) -static -o $(SYMROOT)/$($(STYLE)_ProductName) \
			$($(STYLE)_ALL_OFILES)" ; \
		echo $${CMD} ; $${CMD} || exit 1 ; )
   endif
  endif
 endif
endif

install-man-pages::
	@echo "Installing man pages for $(Project)..."
	@for MANPAGE in $(MANPAGES); do \
		SECTION=$${MANPAGE/*./} ; \
		MANDIR=$(DSTROOT)/usr/share/man/man$${SECTION} ; \
		$(INSTALL_DIRECTORY) $${MANDIR} || exit 1 ; \
		CMD="$(INSTALL_FILE) $${MANPAGE} $${MANDIR}" ; \
		echo $$CMD ; $$CMD || exit 1 ; \
	done

install-launchd-plists::
	@echo "Installing launchd plists for $(Project)..."
	@for PLIST in $(LAUNCHD_PLISTS); do \
		PLIST_DIR=$(DSTROOT)/System/Library/LaunchDaemons ; \
		$(INSTALL_DIRECTORY) $${PLIST_DIR} || exit 1 ; \
		CMD="$(INSTALL_FILE) $${PLIST} $${PLIST_DIR}" ; \
		echo $$CMD ; $$CMD || exit 1 ; \
	done

after_install:

almostclean::
	@echo "Cleaning $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory) clean
