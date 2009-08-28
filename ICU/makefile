##
# Wrapper makefile for ICU
# Copyright (C) 2003-2009 Apple Inc. All rights reserved.
#
# See http://www.gnu.org/manual/make/html_chapter/make_toc.html#SEC_Contents
# for documentation on makefiles. Most of this was culled from the ncurses makefile.
#
##

#################################
#################################
# MAKE VARS
#################################
#################################

# ':=' denotes a "simply expanded" variable. It's value is
# set at the time of definition and it never recursively expands
# when used. This is in contrast to using '=' which denotes a
# recursively expanded variable.

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
WINDOWS=NO
SRCROOT=$(shell pwd)
OBJROOT=$(SRCROOT)/build
DSTROOT=$(OBJROOT)
SYMROOT=$(OBJROOT)
APPLE_INTERNAL_DIR=/AppleInternal
RC_ARCHS=
DISABLE_DRAFT=NO

ifeq "$(DISABLE_DRAFT)" "YES"
	DRAFT_FLAG=--disable-draft
else
	DRAFT_FLAG=
endif

ifeq "$(filter arm armv6,$(RC_ARCHS))" ""
	THUMB_FLAG =
else
	THUMB_FLAG = -mthumb
endif

ifeq "$(RC_INDIGO)" "YES"
	include $(APPLE_INTERNAL_DIR)/Indigo/Makefile.indigo
	ifndef SDKROOT
		SDKROOT=$(INDIGO_PREFIX)
	endif
	DEST_ROOT=$(DSTROOT)/$(INDIGO_PREFIX)/
else
	DEST_ROOT=$(DSTROOT)
endif

TZDATA=$(lastword $(wildcard $(SDKROOT)/usr/local/share/tz/tzdata*.tar.gz))

ifeq "$(WINDOWS)" "NO"
	ifeq "$(SDKROOT)" ""
		EMBEDDED:=$(shell $(CXX) -E -dM -x c -include TargetConditionals.h /dev/null | fgrep TARGET_OS_EMBEDDED | cut -d' ' -f3)
		ISYSROOT =
	else
		EMBEDDED:=$(shell $(CXX) -E -dM -x c -isysroot $(SDKROOT) -include TargetConditionals.h /dev/null | fgrep TARGET_OS_EMBEDDED | cut -d' ' -f3)
		ifeq "$(RC_INDIGO)" "YES"
			ISYSROOT = -isysroot $(SDKROOT)
		else
			ISYSROOT =
		endif
	endif
else
	EMBEDDED:=0
endif

ifeq "$(EMBEDDED)" "1"
	export APPLE_EMBEDDED=YES
else ifeq "$(RC_INDIGO)" "YES"
	export APPLE_EMBEDDED=YES
else
	export APPLE_EMBEDDED=NO
endif

ifndef RC_ProjectSourceVersion
ifdef RC_PROJECTSOURCEVERSION
	RC_ProjectSourceVersion=$(RC_PROJECTSOURCEVERSION)
endif
endif

ifneq "$(RC_ProjectSourceVersion)" ""
	ifeq "$(WINDOWS)" "YES"
		ICU_BUILD := $(shell echo $(RC_ProjectSourceVersion) | sed -r -e 's/[0-9]+.([0-9]+)(.[0-9]+)?/\1/')
	else
		ICU_BUILD := $(shell echo $(RC_ProjectSourceVersion) | sed -E -e 's/[0-9]+.([0-9]+)(.[0-9]+)?/\1/')
	endif
else
	ICU_BUILD := 0
endif

# Disallow $(SRCROOT) == $(OBJROOT)
ifeq ($(OBJROOT), $(SRCROOT))
$(error SRCROOT same as OBJROOT)
endif

#################################
# Headers
#################################

# For installhdrs. Not every compiled module has an associated header. Normally,
# ICU installs headers as a sub-targe of the install target. But since we only want
# certain libraries to install (and since we link all of our own .o modules), we need
# invoke the headers targets ourselves. This may be problematic because there isn't a
# good way to dist-clean afterwards...we need to do explicit dist-cleans, especially if
# install the extra libraries.

EXTRA_HDRS =
# EXTRA_HDRS = ./extra/ustdio/ ./layout/ 
ifeq "$(APPLE_EMBEDDED)" "YES"
    HDR_MAKE_SUBDIR = ./common/ ./i18n/ $(EXTRA_HDRS)
else ifeq "$(WINDOWS)" "YES"
    HDR_MAKE_SUBDIR = ./common/ ./i18n/ $(EXTRA_HDRS)
else
    HDR_MAKE_SUBDIR = ./common/ ./i18n/ ./io/ $(EXTRA_HDRS)
endif
ifeq "$(WINDOWS)" "YES"
	HDR_PREFIX=$(APPLE_INTERNAL_DIR)
	PRIVATE_HDR_PREFIX=$(APPLE_INTERNAL_DIR)
else
	HDR_PREFIX=/usr
	PRIVATE_HDR_PREFIX=/usr/local
endif

#################################
# Install
#################################

# For install. We currently don't install EXTRA_LIBS. We also don't install the data 
# directly into the ICU library. It is now installed at /usr/share/icu/*.dat. Thus we 
# don't use DATA_OBJ anymore. This could change if we decide to move the data back into
# the icucore monolithic library.

INSTALL = /usr/bin/install
COMMON_OBJ = ./common/*.o
I18N_OBJ = ./i18n/*.o
IO_OBJ = ./io/*.o
STUB_DATA_OBJ = ./stubdata/*.o
#COMMON_SRC = $(OBJROOT)/common/*.c
#I18N_SRC = $(OBJROOT)/i18n/*.c
#IO_SRC = $(OBJROOT)/io/*.c
#STUB_DATA_SRC = $(OBJROOT)/stubdata/*.c
EXTRA_LIBS =
#EXTRA_LIBS =./extra/ ./layout/ ./tools/ctestfw/ ./tools/toolutil/
#DATA_OBJ = ./data/out/build/*.o
ifeq "$(APPLE_EMBEDDED)" "YES"
    DYLIB_OBJS=$(COMMON_OBJ) $(I18N_OBJ) $(STUB_DATA_OBJ)
else ifeq "$(WINDOWS)" "YES"
    DYLIB_OBJS=$(COMMON_OBJ) ./common/common.res $(I18N_OBJ) $(STUB_DATA_OBJ)
else
    DYLIB_OBJS=$(COMMON_OBJ) $(I18N_OBJ) $(IO_OBJ) $(STUB_DATA_OBJ)
endif

#################################
# Sources
#################################

# For installsrc (B&I)
# Note that installsrc is run on the system from which ICU is submitted, which
# may be a different environment than the one for a which a build is targeted.

INSTALLSRC_VARFILES=./ICU_embedded.order ./minimalpatchconfig.txt ./windowspatchconfig.txt ./patchconfig.txt

#################################
# Cleaning
#################################

#We need to clean after installing.

EXTRA_CLEAN = 

# Some directories aren't cleaned recursively. Clean them manually...
MANUAL_CLEAN_TOOLS = ./tools/dumpce
MANUAL_CLEAN_EXTRA = ./extra/scrptrun ./samples/layout ./extra/ustdio ./extra
MANUAL_CLEAN_TEST = ./test/collperf ./test/iotest ./test/letest ./test/thaitest ./test/threadtest ./test/testmap ./test 
MANUAL_CLEAN_SAMPLE = ./samples/layout ./samples

CLEAN_SUBDIR = ./stubdata ./common ./i18n ./io ./layout ./layoutex ./data ./tools ./$(MANUAL_CLEAN_TOOLS) $(MANUAL_CLEAN_EXTRA) $(MANUAL_CLEAN_TEST) $(MANUAL_CLEAN_SAMPLE) 

#################################
# Config flags
#################################

ifeq "$(APPLE_EMBEDDED)" "YES"
	CONFIG_FLAGS = --disable-renaming --disable-extras --disable-icuio --disable-layout \
		--disable-samples --with-data-packaging=archive --prefix=$(PRIVATE_HDR_PREFIX) \
		$(DRAFT_FLAG) --srcdir=$(SRCROOT)/icuSources
else ifeq "$(WINDOWS)" "YES"
	CONFIG_FLAGS = --disable-extras --disable-icuio --disable-layout \
		--disable-samples --with-data-packaging=library --prefix=$(PRIVATE_HDR_PREFIX) \
		$(DRAFT_FLAG)
else
	CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout \
		--disable-samples --with-data-packaging=archive --prefix=$(PRIVATE_HDR_PREFIX) \
		$(DRAFT_FLAG) --srcdir=$(SRCROOT)/icuSources
endif

#################################
# Install paths
#################################

# This may or may not be an appropriate name for the icu dylib. This naming scheme is 
# an attempt to follow the icu convention in naming the dylib and then having symbolic 
# links of easier to remember library names point it it. *UPDATE* the version and 
# sub-version variables as needed. The Core version should be 'A' until the core
# version changes it's API...that is a new version isn't backwards compatible.
# The ICU version/subversion should reflect the actual ICU version.

LIB_NAME = icucore
ICU_VERS = 40
ICU_SUBVERS = 0
CORE_VERS = A

ifeq "$(WINDOWS)" "YES"
	DYLIB_SUFF = dll
	libdir = /AppleInternal/bin/
	winlibdir = /AppleInternal/lib/
else
	DYLIB_SUFF = dylib
	libdir = /usr/lib/
	winlibdir =
endif

DYLIB = lib$(LIB_NAME).$(DYLIB_SUFF)
DYLIB_DEBUG = lib$(LIB_NAME)_debug.$(DYLIB_SUFF)
DYLIB_PROFILE = lib$(LIB_NAME)_profile.$(DYLIB_SUFF)
ifeq "$(WINDOWS)" "YES"
	INSTALLED_DYLIB = $(LIB_NAME).$(DYLIB_SUFF)
	INSTALLED_DYLIB_DEBUG = $(LIB_NAME)_debug.$(DYLIB_SUFF)
	INSTALLED_DYLIB_PROFILE = $(LIB_NAME)_profile.$(DYLIB_SUFF)
else
	INSTALLED_DYLIB = lib$(LIB_NAME).$(CORE_VERS).$(DYLIB_SUFF)
	INSTALLED_DYLIB_DEBUG = lib$(LIB_NAME).$(CORE_VERS)_debug.$(DYLIB_SUFF)
	INSTALLED_DYLIB_PROFILE = lib$(LIB_NAME).$(CORE_VERS)_profile.$(DYLIB_SUFF)
endif

INSTALLED_DYLIB_icu = INSTALLED_DYLIB
INSTALLED_DYLIB_debug = INSTALLED_DYLIB_DEBUG
INSTALLED_DYLIB_profile = INSTALLED_DYLIB_PROFILE
DYLIB_icu = DYLIB
DYLIB_debug = DYLIB_DEBUG
DYLIB_profile = DYLIB_PROFILE

#################################
# Data files
#################################

datadir=/usr/share/icu/
OPEN_SOURCE_VERSIONS_DIR=/usr/local/OpenSourceVersions/
OPEN_SOURCE_LICENSES_DIR=/usr/local/OpenSourceLicenses/
ICU_DATA_DIR= data/out
B_DATA_FILE=icudt$(ICU_VERS)b.dat
L_DATA_FILE=icudt$(ICU_VERS)l.dat

#################################
# Environment variables
#################################

# $(RC_ARCHS:%=-arch %) is a substitution reference. It denotes, in this case,
# for every value <val> in RC_ARCHS, replace it with "-arch <val>". Substitution
# references have the form $(var:a=b). We can insert the strip and prebinding commands
# into CFLAGS (and CXXFLAGS). This controls a lot of the external variables so we don't
# need to directly modify the ICU files (like for CFLAGS, etc).

LIBOVERRIDES=LIBICUDT="-L$(OBJROOT) -l$(LIB_NAME)" \
	LIBICUUC="-L$(OBJROOT) -l$(LIB_NAME)" \
	LIBICUI18N="-L$(OBJROOT) -l$(LIB_NAME)"

# For normal Windows builds set the ENV= options here; for debug Windows builds set the ENV_DEBUG=
# options here and also the update the LINK.EXE lines in the TARGETS section below.
ifeq "$(WINDOWS)" "YES"
	ifeq "$(ICU_BUILD)" "0"
		CPPOPTIONS = 
	else
		CPPOPTIONS = CPPFLAGS="-DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD)"
	endif
	ENV= CFLAGS="/O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" CXXFLAGS="/O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t" LDFLAGS="/NXCOMPAT /SAFESEH /DYNAMICBASE"
	ENV_CONFIGURE= $(CPPOPTIONS) CFLAGS="/O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" CXXFLAGS="/O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t"
	ENV_DEBUG= CFLAGS="/O2 /Ob2 /MDd /GF /GS /Zi /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" CXXFLAGS="/O2 /Ob2 /MDd /GF /GS /Zi /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" LDFLAGS="/DEBUG"
	ENV_PROFILE=
else
	ifeq "$(ICU_BUILD)" "0"
		CPPOPTIONS = CPPFLAGS="-DSTD_INSPIRED -DMAC_OS_X_VERSION_MIN_REQUIRED=1040"
	else
		CPPOPTIONS = CPPFLAGS="-DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD) -DSTD_INSPIRED -DMAC_OS_X_VERSION_MIN_REQUIRED=1040"
	endif
	ENV=	APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -g -Os -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -g -Os -fno-exceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
		
	ENV_CONFIGURE=	$(CPPOPTIONS) APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -g -Os -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -g -Os -fno-exceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
	
	ENV_DEBUG = APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -O0 -gfull -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -O0 -gfull -fno-exceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
	
	ENV_PROFILE = APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -g -Os -pg -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -g -Os -pg -fno-exceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
endif
	
ENV_icu = ENV
ENV_debug = ENV_DEBUG
ENV_profile = ENV_PROFILE

ifeq "$(APPLE_EMBEDDED)" "YES"
	ORDERFILE=$(SRCROOT)/ICU_embedded.order
else
	ORDERFILE=/usr/local/lib/OrderFiles/libicucore.order
endif
ifeq "$(shell test -f $(ORDERFILE) && echo YES )" "YES"
       SECTORDER_FLAGS=-sectorder __TEXT __text $(ORDERFILE)
else
       SECTORDER_FLAGS=
endif


#################################
#################################
# TARGETS
#################################
#################################

.PHONY : icu check installsrc installhdrs installhdrsint clean install debug debug-install
.DELETE_ON_ERROR :

icu debug profile : $(OBJROOT)/Makefile
	(cd $(OBJROOT); \
		$(MAKE) $($(ENV_$@)); \
		if test "$(WINDOWS)" = "YES"; then \
			if test "$@" = "debug"; then \
				(cd common; \
					LINK.EXE /subsystem:console /DLL /nologo /base:"0x4a800000" /DEBUG \
						/IMPLIB:../lib/icuuc_$@.lib /out:../lib/icuuc$(ICU_VERS)_$@.dll *.o \
						common.res ../stubdata/icudt.lib advapi32.lib;); \
				(cd i18n; \
					LINK.EXE /subsystem:console /DLL /nologo /base:"0x4a900000" /DEBUG \
						/IMPLIB:../lib/icuin_$@.lib /out:../lib/icuin$(ICU_VERS)_$@.dll *.o \
						i18n.res ../lib/icuuc_$@.lib ../stubdata/icudt.lib advapi32.lib;); \
			fi; \
		else \
			tmpfile=`mktemp -t weakexternal` || exit 1; \
			nm -m $(RC_ARCHS:%=-arch %) $(DYLIB_OBJS) | fgrep "weak external" | fgrep -v "undefined" | sed -e 's/.*weak external[^_]*//' | sort | uniq | cat >$$tmpfile; \
			$($(ENV_$@)) $(CXX) -current_version $(ICU_VERS).$(ICU_SUBVERS) -compatibility_version 1 -dynamiclib -dynamic \
				$(RC_ARCHS:%=-arch %) $(CXXFLAGS) $(LDFLAGS) -single_module $(SECTORDER_FLAGS) -unexported_symbols_list $$tmpfile -dead_strip \
				-install_name $(libdir)$($(INSTALLED_DYLIB_$@)) -o ./$($(INSTALLED_DYLIB_$@)) $(DYLIB_OBJS); \
			if test -f ./$(ICU_DATA_DIR)/$(B_DATA_FILE); then \
				ln -fs ./$(ICU_DATA_DIR)/$(B_DATA_FILE); \
			else \
				DYLD_LIBRARY_PATH="./lib:./stubdata" \
				./bin/icupkg -tb ./$(ICU_DATA_DIR)/$(L_DATA_FILE) $(B_DATA_FILE); \
			fi; \
			if test -f ./$(ICU_DATA_DIR)/$(L_DATA_FILE); then \
				ln -fs ./$(ICU_DATA_DIR)/$(L_DATA_FILE); \
			else \
				DYLD_LIBRARY_PATH="./lib:./stubdata" \
				./bin/icupkg -tl ./$(ICU_DATA_DIR)/$(B_DATA_FILE) $(L_DATA_FILE); \
			fi; \
		fi; \
	);

check : icu
	(cd $(OBJROOT); \
		ICU_DATA=$(OBJROOT) $(MAKE) $(ENV) check; \
	);

check-debug: debug
	(cd $(OBJROOT); \
		ICU_DATA=$(OBJROOT) $(MAKE) $(ENV_DEBUG) check; \
	);

samples: icu
	(cd $(OBJROOT)/samples; \
		$(MAKE) $(ENV_DEBUG) $(LIBOVERRIDES); \
	);

extra: icu
	(cd $(OBJROOT)/extra; \
		$(MAKE) $(ENV_DEBUG) $(LIBOVERRIDES); \
	);

$(OBJROOT)/Makefile : 
	if test ! -d $(OBJROOT); then \
		mkdir -p $(OBJROOT); \
	fi;
	if test "$(WINDOWS)" = "YES"; then \
		cp -Rpf $(SRCROOT)/icuSources/* $(OBJROOT); \
		(cd $(OBJROOT)/data/unidata; mv base_unidata/*.txt .;); \
		(cd $(OBJROOT); $(ENV_CONFIGURE) ./runConfigureICU Cygwin/MSVC $(CONFIG_FLAGS);) \
	else \
		(cd $(OBJROOT); $(ENV_CONFIGURE) $(SRCROOT)/icuSources/runConfigureICU MacOSX $(CONFIG_FLAGS);) \
	fi;
	if test "$(APPLE_EMBEDDED)" = "YES"; then \
		(cd $(OBJROOT)/common/unicode/; \
			cp $(SRCROOT)/icuSources/common/unicode/uconfig.h . ; \
			cp $(SRCROOT)/icuSources/common/unicode/udata.h . ; \
			patch <$(SRCROOT)/minimalpatchconfig.txt;) \
	elif test "$(WINDOWS)" = "YES"; then \
		(cd $(OBJROOT)/common/unicode/; \
			cp $(SRCROOT)/icuSources/common/unicode/uconfig.h . ; \
			patch <$(SRCROOT)/windowspatchconfig.txt;) \
	else \
		(cd $(OBJROOT)/common/unicode/; \
			cp $(SRCROOT)/icuSources/common/unicode/uconfig.h . ; \
			patch <$(SRCROOT)/patchconfig.txt;) \
	fi; \
	if test -f $(SRCROOT)/icuSources/common/Makefile.local; then \
		cp -p $(SRCROOT)/icuSources/common/Makefile.local $(OBJROOT)/common/ ; \
	fi; 

#################################
# B&I TARGETS
#################################

# Since our sources are in icuSources (ignore the ICU subdirectory for now), we wish to 
# copy them to somewhere else. We tar it to stdout, cd to the appropriate directory, and
# untar from stdin.  We then look for all the CVS directories and remove them. We may have 
# to remove the .cvsignore files also.

installsrc :
	if test ! -d $(SRCROOT); then mkdir $(SRCROOT); fi;
	if test -d $(SRCROOT)/icuSources ; then rm -rf $(SRCROOT)/icuSources; fi;
	tar cf - ./makefile ./ICU.plist ./license.html ./icuSources $(INSTALLSRC_VARFILES) | (cd $(SRCROOT) ; tar xfp -); \
	for i in `find $(SRCROOT)/icuSources/ | grep "CVS$$"` ; do \
		if test -d $$i ; then \
			rm -rf $$i; \
		fi; \
	done
	for j in `find $(SRCROOT)/icuSources/ | grep ".cvsignore"` ; do \
		if test -f $$j ; then \
			rm -f $$j; \
		fi; \
	done

# This works. Just not for ~ in the DEST_ROOT. We run configure first (in case it hasn't 
# been already). Then we make the install-headers target on specific makefiles (since 
# not every subdirectory/sub-component has a install-headers target).

# installhdrs should be no-op for RC_INDIGO
ifeq "$(RC_INDIGO)" "YES"
installhdrs :
else
installhdrs : installhdrsint
endif
	
installhdrsint : $(OBJROOT)/Makefile
	(cd $(OBJROOT); \
		for subdir in $(HDR_MAKE_SUBDIR); do \
			(cd $$subdir; $(MAKE) -e DESTDIR=$(DEST_ROOT) $(ENV) install-headers); \
		done; \
	);

# We run configure and run make first. This generates the .o files. We then link them 
# all up together into libicucore. Then we put it into its install location, create 
# symbolic links, and then strip the main dylib. Then install the remaining libraries. 
# We cleanup the sources folder.

install : installhdrsint icu
	if test ! -d $(DEST_ROOT)$(libdir)/; then \
		$(INSTALL) -d -m 0755 $(DEST_ROOT)$(libdir)/; \
	fi;
	if test "$(WINDOWS)" = "YES"; then \
		if test ! -d $(DEST_ROOT)$(winlibdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(winlibdir)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuuc.lib $(DEST_ROOT)$(winlibdir)icuuc.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuuc$(ICU_VERS).pdb $(DEST_ROOT)$(libdir)icuuc$(ICU_VERS).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/icuuc$(ICU_VERS).dll $(DEST_ROOT)$(libdir)icuuc$(ICU_VERS).dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuin.lib $(DEST_ROOT)$(winlibdir)icuin.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuin$(ICU_VERS).pdb $(DEST_ROOT)$(libdir)icuin$(ICU_VERS).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/icuin$(ICU_VERS).dll $(DEST_ROOT)$(libdir)icuin$(ICU_VERS).dll; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/icudt$(ICU_VERS).dll $(DEST_ROOT)$(libdir)icudt$(ICU_VERS).dll; \
	else \
		$(INSTALL) -b -m 0664 $(OBJROOT)/$(INSTALLED_DYLIB) $(DEST_ROOT)$(libdir)$(INSTALLED_DYLIB); \
		(cd $(DEST_ROOT)$(libdir); \
		ln -fs  $(INSTALLED_DYLIB) $(DYLIB)); \
		cp $(OBJROOT)/$(INSTALLED_DYLIB) $(SYMROOT)/$(INSTALLED_DYLIB); \
		strip -x -u -r -S $(DEST_ROOT)$(libdir)$(INSTALLED_DYLIB); \
		for subdir in $(EXTRA_LIBS); do \
			(cd $(OBJROOT)/$$subdir; $(MAKE) -e DESTDIR=$(DEST_ROOT) $(ENV) install-library;) \
		done; \
		if test ! -d $(DEST_ROOT)$(datadir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(datadir)/; \
		fi;	\
		if test -f $(OBJROOT)/$(B_DATA_FILE); then \
			$(INSTALL) -b -m 0644  $(OBJROOT)/$(B_DATA_FILE) $(DEST_ROOT)$(datadir)$(B_DATA_FILE); \
		fi; \
		if test -f $(OBJROOT)/$(L_DATA_FILE); then \
			$(INSTALL) -b -m 0644  $(OBJROOT)/$(L_DATA_FILE) $(DEST_ROOT)$(datadir)$(L_DATA_FILE); \
		fi; \
		if test ! -d $(DEST_ROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(SRCROOT)/ICU.plist $(DEST_ROOT)$(OPEN_SOURCE_VERSIONS_DIR)ICU.plist; \
		if test ! -d $(DEST_ROOT)$(OPEN_SOURCE_LICENSES_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(OPEN_SOURCE_LICENSES_DIR)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(SRCROOT)/license.html $(DEST_ROOT)$(OPEN_SOURCE_LICENSES_DIR)ICU.html; \
	fi;

DEPEND_install_debug = debug
DEPEND_install_profile = profile
	
.SECONDEXPANSION:
install_debug install_profile : $$(DEPEND_$$@)
	if test ! -d $(DEST_ROOT)$(libdir)/; then \
		$(INSTALL) -d -m 0755 $(DEST_ROOT)$(libdir)/; \
	fi;
	if test "$(WINDOWS)" = "YES"; then \
		if test ! -d $(DEST_ROOT)$(winlibdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(winlibdir)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuuc_$(DEPEND_$@).lib $(DEST_ROOT)$(winlibdir)icuuc_$(DEPEND_$@).lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuuc$(ICU_VERS)_$(DEPEND_$@).pdb $(DEST_ROOT)$(libdir)icuuc$(ICU_VERS)_$(DEPEND_$@).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/icuuc$(ICU_VERS)_$(DEPEND_$@).dll $(DEST_ROOT)$(libdir)icuuc$(ICU_VERS)_$(DEPEND_$@).dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuin_$(DEPEND_$@).lib $(DEST_ROOT)$(winlibdir)icuin_$(DEPEND_$@).lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuin$(ICU_VERS)_$(DEPEND_$@).pdb $(DEST_ROOT)$(libdir)icuin$(ICU_VERS)_$(DEPEND_$@).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/icuin$(ICU_VERS)_$(DEPEND_$@).dll $(DEST_ROOT)$(libdir)icuin$(ICU_VERS)_$(DEPEND_$@).dll; \
	else \
		$(INSTALL) -b -m 0664 $(OBJROOT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(DEST_ROOT)$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		(cd $(DEST_ROOT)$(libdir); \
		ln -fs  $($(INSTALLED_DYLIB_$(DEPEND_$@))) $($(DYLIB_$(DEPEND_$@)))); \
		cp $(OBJROOT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(SYMROOT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		strip -x -u -r -S $(DEST_ROOT)$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		for subdir in $(EXTRA_LIBS); do \
			(cd $(OBJROOT)/$$subdir; $(MAKE) -e DESTDIR=$(DEST_ROOT) $(ENV) install-library;) \
		done; \
	fi;

clean :
	-rm -rf $(OBJROOT)

