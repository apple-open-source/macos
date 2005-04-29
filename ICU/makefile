##
# Makefile for ICU
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
SRCROOT=$(shell pwd)
OBJROOT=$(SRCROOT)/build
DSTROOT=$(OBJROOT)
SYMROOT=$(OBJROOT)
APPLE_INTERNAL_DIR=/AppleInternal
RC_ARCHS=

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
HDR_MAKE_SUBDIR = ./common/ ./i18n/ $(EXTRA_HDRS)

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
STUB_DATA_OBJ = ./stubdata/*.o
COMMON_SRC = $(OBJROOT)/common/*.c
I18N_SRC = $(OBJROOT)/i18n/*.c
STUB_DATA_SRC = $(OBJROOT)/stubdata/*.c
EXTRA_LIBS =
#EXTRA_LIBS =./extra/ ./layout/ ./tools/ctestfw/ ./tools/toolutil/
#DATA_OBJ = ./data/out/build/*.o

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

CLEAN_SUBDIR = ./stubdata ./common ./i18n ./layout ./layoutex ./data ./tools ./$(MANUAL_CLEAN_TOOLS) $(MANUAL_CLEAN_EXTRA) $(MANUAL_CLEAN_TEST) $(MANUAL_CLEAN_SAMPLE) 

#################################
# Config flags
#################################

CONFIG_FLAGS = --disable-renaming --disable-extras --disable-ustdio --disable-layout \
	--disable-samples --with-data-packaging=archive --prefix=/usr/local \
	--srcdir=$(SRCROOT)/icuSources

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
ICU_VERS = 32
ICU_SUBVERS = 0
CORE_VERS = A
DYLIB_SUFF = dylib
libdir = /usr/lib/

INSTALLED_DYLIB = lib$(LIB_NAME).$(CORE_VERS).$(DYLIB_SUFF)
DYLIB = lib$(LIB_NAME).$(DYLIB_SUFF)

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

ENV=	APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
	CFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -g -Os -fno-exceptions" \
	CXXFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -g -Os -fno-exceptions -fno-rtti" \
	RC_ARCHS="$(RC_ARCHS)" \
	DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"
	
ENV_CONFIGURE=	APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
	CFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -g -Os -fno-exceptions" \
	CXXFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -g -Os -fno-exceptions -fno-rtti" \
	RC_ARCHS="$(RC_ARCHS)" \
	DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

ENV_DEBUG = APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
	CFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -O0 -g -fno-exceptions" \
	CXXFLAGS="-DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) -O0 -g -fno-exceptions -fno-rtti" \
	RC_ARCHS="$(RC_ARCHS)" \
	DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

ENV_icu = ENV
ENV_debug = ENV_DEBUG

ORDERFILE=/usr/local/lib/OrderFiles/libicucore.order
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

.PHONY : icu check installsrc installhdrs clean install debug debug-install
.DELETE_ON_ERROR :

icu debug : $(OBJROOT)/Makefile
	(cd $(OBJROOT); \
		$(MAKE) $($(ENV_$@)); \
		$($(ENV_$@)) $(CXX) -current_version $(ICU_VERS).$(ICU_SUBVERS) -compatibility_version 1 -dynamiclib -dynamic \
			$(RC_ARCHS:%=-arch %) $(CXXFLAGS) $(LDFLAGS) -single_module $(SECTORDER_FLAGS) \
			-install_name $(libdir)$(INSTALLED_DYLIB) -o ./$(INSTALLED_DYLIB) $(COMMON_OBJ) $(I18N_OBJ) $(STUB_DATA_OBJ); \
		if test -f ./$(ICU_DATA_DIR)/$(B_DATA_FILE); then \
			ln -fs ./$(ICU_DATA_DIR)/$(B_DATA_FILE); \
		else \
			DYLD_LIBRARY_PATH="./lib:./stubdata" \
			./bin/icuswap -tb ./$(ICU_DATA_DIR)/$(L_DATA_FILE) $(B_DATA_FILE); \
		fi; \
		if test -f ./$(ICU_DATA_DIR)/$(L_DATA_FILE); then \
		    ln -fs ./$(ICU_DATA_DIR)/$(L_DATA_FILE); \
		else \
			DYLD_LIBRARY_PATH="./lib:./stubdata" \
			./bin/icuswap -tl ./$(ICU_DATA_DIR)/$(B_DATA_FILE) $(L_DATA_FILE); \
		fi; \
	);

check : icu
	(cd $(OBJROOT); \
		$(MAKE) $(ENV) check; \
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
	(cd $(OBJROOT); $(ENV_CONFIGURE) $(SRCROOT)/icuSources/configure $(CONFIG_FLAGS);)

debug-install:	debug installhdrs
	if test ! -d $(DSTROOT)$(libdir)/; then \
		$(INSTALL) -d -m 0775 $(DSTROOT)$(libdir)/; \
	fi;
	$(INSTALL) -b -m 0664 $(OBJROOT)/$(INSTALLED_DYLIB) $(DSTROOT)$(libdir)$(INSTALLED_DYLIB)
	(cd $(DSTROOT)$(libdir); \
	ln -fs  $(INSTALLED_DYLIB) $(DYLIB));
	for subdir in $(EXTRA_LIBS); do \
		(cd $(OBJROOT)/$$subdir; $(MAKE) -e DESTDIR=$(DSTROOT) $(ENV_DEBUG) install-library;) \
	done;
	if test ! -d $(DSTROOT)$(datadir)/; then \
		$(INSTALL) -d -m 0755 $(DSTROOT)$(datadir)/; \
	fi;	
	if test -f $(OBJROOT)/$(B_DATA_FILE); then \
		$(INSTALL) -b -m 0644  $(OBJROOT)/$(B_DATA_FILE) $(DSTROOT)$(datadir)$(B_DATA_FILE); \
	fi;
	if test -f $(OBJROOT)/$(L_DATA_FILE); then \
		$(INSTALL) -b -m 0644  $(OBJROOT)/$(L_DATA_FILE) $(DSTROOT)$(datadir)$(L_DATA_FILE); \
	fi;
	if test ! -d $(DSTROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; then \
		$(INSTALL) -d -m 0755 $(DSTROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; \
	fi;
	$(INSTALL) -b -m 0644 $(SRCROOT)/ICU.plist $(DSTROOT)$(OPEN_SOURCE_VERSIONS_DIR)ICU.plist;
	if test ! -d $(DSTROOT)$(OPEN_SOURCE_LICENSES_DIR)/; then \
		$(INSTALL) -d -m 0755 $(DSTROOT)$(OPEN_SOURCE_LICENSES_DIR)/; \
	fi;
	$(INSTALL) -b -m 0644 $(SRCROOT)/license.html $(DSTROOT)$(OPEN_SOURCE_LICENSES_DIR)ICU.html;

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
	tar cf - ./makefile ./ICU.plist ./license.html ./icuSources | (cd $(SRCROOT) ; tar xfp -); \
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

# This works. Just not for ~ in the DSTROOT. We run configure first (in case it hasn't 
# been already). Then we make the install-headers target on specific makefiles (since 
# not every subdirectory/sub-component has a install-headers target).

installhdrs : $(OBJROOT)/Makefile
	(cd $(OBJROOT); \
	for subdir in $(HDR_MAKE_SUBDIR); do \
		(cd $$subdir; $(MAKE) -e DESTDIR=$(DSTROOT) $(ENV) install-headers); \
	done;);

# We run configure and run make first. This generates the .o files. We then link them 
# all up together into libicucore. Then we put it into its install location, create 
# symbolic links, and then strip the main dylib. Then install the remaining libraries. 
# We cleanup the sources folder.
	
install : installhdrs icu
	if test ! -d $(DSTROOT)$(libdir)/; then \
		$(INSTALL) -d -m 0775 $(DSTROOT)$(libdir)/; \
	fi;
	$(INSTALL) -b -m 0664 $(OBJROOT)/$(INSTALLED_DYLIB) $(DSTROOT)$(libdir)$(INSTALLED_DYLIB)
	(cd $(DSTROOT)$(libdir); \
	ln -fs  $(INSTALLED_DYLIB) $(DYLIB));
	cp $(OBJROOT)/$(INSTALLED_DYLIB) $(SYMROOT)/$(INSTALLED_DYLIB);
	strip -x -u -r -S $(DSTROOT)$(libdir)$(INSTALLED_DYLIB);
	for subdir in $(EXTRA_LIBS); do \
		(cd $(OBJROOT)/$$subdir; $(MAKE) -e DESTDIR=$(DSTROOT) $(ENV) install-library;) \
	done;
	if test ! -d $(DSTROOT)$(datadir)/; then \
		$(INSTALL) -d -m 0755 $(DSTROOT)$(datadir)/; \
	fi;	
	if test -f $(OBJROOT)/$(B_DATA_FILE); then \
		$(INSTALL) -b -m 0644  $(OBJROOT)/$(B_DATA_FILE) $(DSTROOT)$(datadir)$(B_DATA_FILE); \
	fi;
	if test -f $(OBJROOT)/$(L_DATA_FILE); then \
		$(INSTALL) -b -m 0644  $(OBJROOT)/$(L_DATA_FILE) $(DSTROOT)$(datadir)$(L_DATA_FILE); \
	fi;
	if test ! -d $(DSTROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; then \
		$(INSTALL) -d -m 0755 $(DSTROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; \
	fi;
	$(INSTALL) -b -m 0644 $(SRCROOT)/ICU.plist $(DSTROOT)$(OPEN_SOURCE_VERSIONS_DIR)ICU.plist;
	if test ! -d $(DSTROOT)$(OPEN_SOURCE_LICENSES_DIR)/; then \
		$(INSTALL) -d -m 0755 $(DSTROOT)$(OPEN_SOURCE_LICENSES_DIR)/; \
	fi;
	$(INSTALL) -b -m 0644 $(SRCROOT)/license.html $(DSTROOT)$(OPEN_SOURCE_LICENSES_DIR)ICU.html;

clean :
	-rm -rf $(OBJROOT)
