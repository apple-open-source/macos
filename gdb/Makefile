GDB_VERSION = 6.1-20040303
GDB_RC_VERSION = 437

BINUTILS_VERSION = 2.13-20021117
BINUTILS_RC_VERSION = 46

.PHONY: all clean configure build install installsrc installhdrs headers \
	build-core build-binutils build-gdb \
	install-frameworks-headers\
	install-frameworks-macosx \
	install-binutils-macosx \
	install-gdb-fat \
	install-chmod-macosx \
	install-clean install-source check


# Get the correct setting for SYSTEM_DEVELOPER_TOOLS_DOC_DIR if 
# the platform-variables.make file exists.

OS=MACOS
-include /Developer/Makefiles/pb_makefiles/platform-variables.make
ifndef SYSTEM_DEVELOPER_TOOLS_DOC_DIR
SYSTEM_DEVELOPER_TOOLS_DOC_DIR=/Developer/Documentation/DeveloperTools
endif


ifndef RC_ARCHS
RC_ARCHS=$(shell /usr/bin/arch)
endif

ifndef SRCROOT
SRCROOT=.
endif

ifndef OBJROOT
OBJROOT=./obj
endif

ifndef SYMROOT
SYMROOT=./sym
endif

ifndef DSTROOT
DSTROOT=./dst
endif

INSTALL=$(SRCROOT)/src/install-sh

CANONICAL_ARCHS := $(foreach arch,$(RC_ARCHS),$(foreach os,$(RC_OS),$(foreach release,$(RC_RELEASE),$(os):$(arch):$(release))))

CANONICAL_ARCHS := $(subst macos:i386:$(RC_RELEASE),i386-apple-darwin,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst macos:ppc:$(RC_RELEASE),powerpc-apple-darwin,$(CANONICAL_ARCHS))

CANONICAL_ARCHS := $(subst powerpc-apple-darwin i386-apple-darwin,i386-apple-darwin powerpc-apple-darwin,$(CANONICAL_ARCHS))

SRCTOP = $(shell cd $(SRCROOT) && pwd)
OBJTOP = $(shell (test -d $(OBJROOT) || $(INSTALL) -c -d $(OBJROOT)) && cd $(OBJROOT) && pwd)
SYMTOP = $(shell (test -d $(SYMROOT) || $(INSTALL) -c -d $(SYMROOT)) && cd $(SYMROOT) && pwd)
DSTTOP = $(shell (test -d $(DSTROOT) || $(INSTALL) -c -d $(DSTROOT)) && cd $(DSTROOT) && pwd)

ARCH_SAYS := $(shell /usr/bin/arch)
ifeq (i386,$(ARCH_SAYS))
BUILD_ARCH := i386-apple-darwin
else
ifeq (ppc,$(ARCH_SAYS))
BUILD_ARCH := powerpc-apple-darwin
else
BUILD_ARCH := $(ARCH_SAYS)
endif
endif

GDB_VERSION_STRING = $(GDB_VERSION) (Apple version gdb-$(GDB_RC_VERSION))
BINUTILS_VERSION_STRING = "$(BINUTILS_VERSION) (Apple version binutils-$(BINUTILS_RC_VERSION))"

GDB_BINARIES = gdb
GDB_FRAMEWORKS = gdb
GDB_MANPAGES = 

BINUTILS_FRAMEWORKS = bfd binutils
BINUTILS_MANPAGES = 

FRAMEWORKS = $(GDB_FRAMEWORKS) $(BINUTILS_FRAMEWORKS)

ifndef BINUTILS_BUILD_ROOT
BINUTILS_BUILD_ROOT = $(NEXT_ROOT)
endif

ifneq ($(findstring macosx,$(CANONICAL_ARCHS))$(findstring darwin,$(CANONICAL_ARCHS)),)
BINUTILS_FRAMEWORK_PATH = $(BINUTILS_BUILD_ROOT)/System/Library/PrivateFrameworks
BINUTILS_LIB_PATH = $(BINUTILS_BUILD_ROOT)/usr/lib
else
BINUTILS_FRAMEWORK_PATH = $(BINUTILS_BUILD_ROOT)/Library/PrivateFrameworks
BINUTILS_LIB_PATH = $(BINUTILS_BUILD_ROOT)/usr/lib
endif

BFD_FRAMEWORK = $(BINUTILS_FRAMEWORK_PATH)/bfd.framework
BFD_HEADERS = $(BFD_FRAMEWORK)/Headers

LIBERTY_FRAMEWORK = $(BINUTILS_FRAMEWORK_PATH)/liberty.framework
LIBERTY_HEADERS = $(LIBERTY_FRAMEWORK)/Headers

MMALLOC_FRAMEWORK = $(BINUTILS_FRAMEWORK_PATH)/mmalloc.framework
MMALLOC_HEADERS = $(MMALLOC_FRAMEWORK)/Headers

OPCODES_FRAMEWORK = $(BINUTILS_FRAMEWORK_PATH)/opcodes.framework
OPCODES_HEADERS = $(OPCODES_FRAMEWORK)/Headers

BINUTILS_FRAMEWORK = $(BINUTILS_FRAMEWORK_PATH)/binutils.framework
BINUTILS_HEADERS = $(BINUTILS_FRAMEWORK)/Headers

INTL_FRAMEWORK = $(BINUTILS_BUILD_ROOT)/usr/lib/libintl.dylib
INTL_HEADERS = $(BINUTILS_BUILD_ROOT)/usr/include

TAR = gnutar
CPP = cpp
CC = cc
CXX = c++
LD = ld
AR = ar
RANLIB = ranlib
NM = nm
CC_FOR_BUILD = cc

CDEBUGFLAGS = -g
CFLAGS = $(CDEBUGFLAGS) $(RC_CFLAGS)
HOST_ARCHITECTURE = UNKNOWN

RC_CFLAGS_NOARCH = $(strip $(shell echo $(RC_CFLAGS) | sed -e 's/-arch [a-z0-9]*//g'))

SYSTEM_FRAMEWORK = -L../intl -L./intl -L../intl/.libs -L./intl/.libs -lintl -framework System
FRAMEWORK_PREFIX =
FRAMEWORK_SUFFIX =
FRAMEWORK_VERSION = A
FRAMEWORK_VERSION_SUFFIX =

CONFIG_DIR=UNKNOWN
CONF_DIR=UNKNOWN
DEVEXEC_DIR=UNKNOWN
LIBEXEC_BINUTILS_DIR=UNKNOWN
LIBEXEC_GDB_DIR=UNKNOWN
LIBEXEC_LIB_DIR=UNKNOWN
MAN_DIR=UNKNOWN
PRIVATE_FRAMEWORKS_DIR=UNKNOWN

NATIVE_TARGET = unknown--unknown

NATIVE_TARGETS = $(foreach arch1,$(CANONICAL_ARCHS),$(arch1)--$(arch1))

CROSS_TARGETS = $(foreach arch1,$(CANONICAL_ARCHS),$(foreach arch2,$(filter-out $(arch1),$(CANONICAL_ARCHS)),$(arch1)--$(arch2)))

PPC_TARGET=UNKNOWN
I386_TARGET=UNKNOWN

CONFIG_VERBOSE=-v
CONFIG_ENABLE_GDBTK=--enable-gdbtk=no
CONFIG_ENABLE_GDBMI=
CONFIG_ENABLE_BUILD_WARNINGS=--enable-build-warnings
CONFIG_ENABLE_TUI=--disable-tui
CONFIG_ALL_BFD_TARGETS=
CONFIG_ALL_BFD_TARGETS=
CONFIG_64_BIT_BFD=--enable-64-bit-bfd
CONFIG_WITH_MMAP=--with-mmap
CONFIG_WITH_MMALLOC=--with-mmalloc
CONFIG_ENABLE_SHARED=--disable-shared
CONFIG_MAINTAINER_MODE=
CONFIG_BUILD=--build=$(BUILD_ARCH)
CONFIG_OTHER_OPTIONS=--disable-serial-configure

ifneq ($(findstring macosx,$(CANONICAL_ARCHS))$(findstring darwin,$(CANONICAL_ARCHS)),)
CC = cc -arch $(HOST_ARCHITECTURE) -no-cpp-precomp
CC_FOR_BUILD = NEXT_ROOT= cc -no-cpp-precomp
ifeq ($(CONFIG_ENABLE_SHARED),--enable-shared)
    CDEBUGFLAGS = -g -Os 
else
    CDEBUGFLAGS = -g -Os -mdynamic-no-pic
endif

CFLAGS = $(strip $(RC_CFLAGS_NOARCH) $(CDEBUGFLAGS) -Wall -Wimplicit -Wno-long-double)
HOST_ARCHITECTURE = $(shell echo $* | sed -e 's/--.*//' -e 's/powerpc/ppc/' -e 's/-apple-macosx.*//' -e 's/-apple-macos.*//' -e 's/-apple-darwin.*//')
endif

ifneq ($(findstring hpux,$(CANONICAL_ARCHS)),)
CC = gcc
CC_FOR_BUILD = NEXT_ROOT= cc
CDEBUGFLAGS = -g -O3
CFLAGS = $(CDEBUGFLAGS) -D__STDC_EXT__=1 $(RC_CFLAGS_NOARCH)
endif

ifneq ($(findstring solaris,$(CANONICAL_ARCHS)),)
CC = gcc
CC_FOR_BUILD = gcc
CDEBUGFLAGS = -g -O3
CFLAGS = $(CDEBUGFLAGS) $(RC_CFLAGS_NOARCH)
endif

ifneq ($(findstring hpux,$(CANONICAL_ARCHS)),)
SYSTEM_FRAMEWORK =
FRAMEWORK_PREFIX = lib
FRAMEWORK_SUFFIX = .sl
FRAMEWORK_VERSION_SUFFIX = .$(FRAMEWORK_VERSION)
endif

ifneq ($(findstring solaris,$(CANONICAL_ARCHS)),)
CFLAGS = $(CDEBUGFLAGS) $(RC_CFLAGS_NOARCH)
SYSTEM_FRAMEWORK =
FRAMEWORK_PREFIX = lib
FRAMEWORK_SUFFIX = .so
FRAMEWORK_VERSION_SUFFIX = .so.$(FRAMEWORK_VERSION)
endif

ifneq ($(findstring hpux,$(CANONICAL_ARCHS)),)
CONFIG_64_BIT_BFD=
CONFIG_MAINTAINER_MODE=
CONFIG_WITH_MMAP=
CONFIG_WITH_MMALLOC=
endif

ifneq ($(findstring solaris,$(CANONICAL_ARCHS)),)
CONFIG_64_BIT_BFD=
CONFIG_MAINTAINER_MODE=
CONFIG_WITH_MMAP=
CONFIG_WITH_MMALLOC=
endif

MACOSX_FLAGS = \
	CONFIG_DIR=private/etc \
	CONF_DIR=usr/share/gdb \
	DEVEXEC_DIR=usr/bin \
	LIBEXEC_BINUTILS_DIR=usr/libexec/binutils \
	LIBEXEC_GDB_DIR=usr/libexec/gdb \
	LIB_DIR=usr/lib \
	MAN_DIR=usr/share/man \
	PRIVATE_FRAMEWORKS_DIR=System/Library/PrivateFrameworks \
	SOURCE_DIR=System/Developer/Source/Commands/gdb

PDO_FLAGS = \
	CONFIG_DIR=Developer/Libraries/gdb \
	CONF_DIR=Developer/Libraries/gdb \
	DEVEXEC_DIR=Developer/Executables \
	LIBEXEC_BINUTILS_DIR=Developer/Libraries/binutils \
	LIBEXEC_GDB_DIR=Developer/Libraries/gdb \
	MAN_DIR=Local/man \
	PRIVATE_FRAMEWORKS_DIR=Library/PrivateFrameworks \
	LIBEXEC_LIB_DIR=Library/Executables \
	SOURCE_DIR=Developer/Source/gdb

CONFIGURE_OPTIONS = \
	$(CONFIG_VERBOSE) \
	$(CONFIG_ENABLE_GDBTK) \
	$(CONFIG_ENABLE_GDBMI) \
	$(CONFIG_ENABLE_BUILD_WARNINGS) \
	$(CONFIG_ENABLE_TUI) \
	$(CONFIG_ALL_BFD_TARGETS) \
	$(CONFIG_ALL_BFD_TARGETS) \
	$(CONFIG_64_BIT_BFD) \
	$(CONFIG_WITH_MMAP) \
	$(CONFIG_WITH_MMALLOC) \
	$(CONFIG_ENABLE_SHARED) \
	$(CONFIG_MAINTAINER_MODE) \
	$(CONFIG_BUILD) \
	$(CONFIG_OTHER_OPTIONS)

MAKE_OPTIONS = \
	prefix='/usr'

EFLAGS = \
	CFLAGS='$(CFLAGS)' \
	CC='$(CC)' \
	CPP='$(CPP)' \
	CXX='$(CXX)' \
	LD='$(LD)' \
	AR='$(AR)' \
	RANLIB='$(RANLIB)' \
	NM='$(NM)' \
	CC_FOR_BUILD='$(CC_FOR_BUILD)' \
	HOST_ARCHITECTURE='$(HOST_ARCHITECTURE)' \
	NEXT_ROOT='$(NEXT_ROOT)' \
	BINUTILS_FRAMEWORK_PATH='$(BINUTILS_FRAMEWORK_PATH)' \
	SRCROOT='$(SRCROOT)' \
	$(MAKE_OPTIONS)

SFLAGS = $(EFLAGS)

FFLAGS = \
	$(SFLAGS) \
	SYSTEM_FRAMEWORK='$(SYSTEM_FRAMEWORK)' \
	FRAMEWORK_PREFIX='$(FRAMEWORK_PREFIX)' \
	FRAMEWORK_SUFFIX='$(FRAMEWORK_SUFFIX)' \
	FRAMEWORK_VERSION_SUFFIX='$(FRAMEWORK_VERSION_SUFFIX)'

ifeq ($(CONFIG_ENABLE_SHARED),--enable-shared)
FRAMEWORK_TARGET=stamp-framework
framework=-F$(BINUTILS_FRAMEWORKS_PATH) -framework $(1)
else
FRAMEWORK_TARGET=stamp-framework-headers all
framework=-L../$(patsubst liberty,libiberty,$(1)) -l$(patsubst liberty,iberty,$(1))
endif

ifeq ($(CONFIG_ENABLE_SHARED),--enable-shared)
FSFLAGS = \
	$(SFLAGS) \
	MMALLOC_DEP='$(MMALLOC_FRAMEWORK)/mmalloc' \
	MMALLOC='$(call framework,mmalloc)' \
	MMALLOC_CFLAGS='-I$(MMALLOC_HEADERS)' \
	OPCODES_DEP='$(OPCODES_FRAMEWORK)/opcodes' \
	OPCODES='$(call framework,opcodes)' \
	OPCODES_CFLAGS='-I$(OPCODES_HEADERS)' \
	BFD_DIR='$(BFD_HEADERS)' \
	BFD_SRC='$(BFD_HEADERS)' \
	BFD_DEP='$(BFD_FRAMEWORK)/bfd' \
	BFD='$(call framework,bfd)' \
	BFD_CFLAGS='-I$(BFD_HEADERS)' \
	LIBIBERTY_DEP='$(LIBERTY_FRAMEWORK)/liberty' \
	LIBIBERTY='$(call framework,liberty)' \
	LIBIBERTY_CFLAGS='-I$(LIBERTY_HEADERS)' \
	INTL_DEP='$(INTL_FRAMEWORK)' \
	INTL='$(INTL_FRAMEWORK)' \
	INCLUDE_DIR='$(BINUTILS_HEADERS)' \
	INCLUDE_CFLAGS='-I$(BINUTILS_HEADERS)'
else
FSFLAGS = \
	$(SFLAGS)
endif

CONFIGURE_ENV = $(EFLAGS)
MAKE_ENV = $(EFLAGS)

SUBMAKE = $(MAKE_ENV) $(MAKE)

_all: all

$(OBJROOT)/%/stamp-rc-configure:
	$(RM) -r $(OBJROOT)/$*
	$(INSTALL) -c -d $(OBJROOT)/$*
	(cd $(OBJROOT)/$* && \
		$(CONFIGURE_ENV) $(SRCTOP)/src/configure \
			--host=$(shell echo $* | sed -e 's/--.*//') \
			--target=$(shell echo $* | sed -e 's/.*--//') \
			$(CONFIGURE_OPTIONS) \
			)
	touch $@

$(OBJROOT)/%/stamp-rc-configure-cross:
	$(RM) -r $(OBJROOT)/$*
	$(INSTALL) -c -d $(OBJROOT)/$*
	(cd $(OBJROOT)/$* && \
		$(CONFIGURE_ENV) $(SRCTOP)/src/configure \
			--host=$(shell echo $* | sed -e 's/--.*//') \
			--target=$(shell echo $* | sed -e 's/.*--//') \
			$(CONFIGURE_OPTIONS) \
			)
	touch $@

$(OBJROOT)/%/stamp-build-headers:
	$(SUBMAKE) -C $(OBJROOT)/$* configure-intl configure-mmalloc configure-libiberty configure-bfd configure-opcodes configure-gdb
	$(SUBMAKE) -C $(OBJROOT)/$*/mmalloc $(FFLAGS) stamp-framework-headers 
	$(SUBMAKE) -C $(OBJROOT)/$*/libiberty $(FFLAGS) stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$*/bfd $(FFLAGS) headers stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$*/opcodes $(FFLAGS) stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-headers-binutils
	$(SUBMAKE) -C $(OBJROOT)/$*/gdb/doc $(MFLAGS) VERSION='$(GDB_VERSION_STRING)'
	$(SUBMAKE) -C $(OBJROOT)/$* $(MFLAGS) stamp-framework-headers-gdb
	#touch $@

$(OBJROOT)/%/stamp-build-core:
	$(SUBMAKE) -C $(OBJROOT)/$* configure-intl configure-mmalloc configure-libiberty configure-bfd configure-opcodes
	$(SUBMAKE) -C $(OBJROOT)/$*/intl $(SFLAGS) libintl.la
	$(SUBMAKE) -C $(OBJROOT)/$*/mmalloc $(FFLAGS) $(FRAMEWORK_TARGET)
	$(SUBMAKE) -C $(OBJROOT)/$*/libiberty $(FFLAGS) $(FRAMEWORK_TARGET)
	$(SUBMAKE) -C $(OBJROOT)/$*/bfd $(FFLAGS) headers
	$(SUBMAKE) -C $(OBJROOT)/$*/bfd $(FFLAGS) $(FRAMEWORK_TARGET)
	$(SUBMAKE) -C $(OBJROOT)/$*/opcodes $(FFLAGS) $(FRAMEWORK_TARGET)
	$(SUBMAKE) -C $(OBJROOT)/$* configure-readline configure-intl
	$(SUBMAKE) -C $(OBJROOT)/$*/readline $(MFLAGS) all $(FRAMEWORK_TARGET)
	$(SUBMAKE) -C $(OBJROOT)/$*/intl $(MFLAGS)
	#touch $@

$(OBJROOT)/%/stamp-build-binutils:
	$(SUBMAKE) -C $(OBJROOT)/$* configure-binutils
	$(SUBMAKE) -C $(OBJROOT)/$*/binutils $(FSFLAGS) VERSION='$(BINUTILS_VERSION)' VERSION_STRING='$(BINUTILS_VERSION_STRING)' all
ifeq ($(CONFIG_ENABLE_SHARED),--enable-shared)
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-binutils
else
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-headers-binutils
endif
	#touch $@

$(OBJROOT)/%/stamp-build-gdb:
	$(SUBMAKE) -C $(OBJROOT)/$* configure-gdb
	$(SUBMAKE) -C $(OBJROOT)/$*/gdb -W version.in $(MFLAGS) $(FSFLAGS) VERSION='$(GDB_VERSION_STRING)' gdb

$(OBJROOT)/%/stamp-build-gdb-framework:
ifeq ($(CONFIG_ENABLE_SHARED),--enable-shared)
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-gdb
else
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-headers-gdb
endif

$(OBJROOT)/%/stamp-build-gdb-docs:
	$(SUBMAKE) -C $(OBJROOT)/$*/gdb/doc $(MFLAGS) VERSION='$(GDB_VERSION_STRING)' gdb.info
	#touch $@

TEMPLATE_HEADERS = config.h tm.h xm.h nm.h

install-frameworks-headers:

	$(INSTALL) -c -d $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)

	set -e;	for i in $(FRAMEWORKS); do \
		framedir=$(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework; \
		$(INSTALL) -c -d $${framedir}/Versions/A/PrivateHeaders; \
		$(INSTALL) -c -d $${framedir}/Versions/A/Headers; \
		ln -sf A $${framedir}/Versions/Current; \
		ln -sf Versions/Current/PrivateHeaders $${framedir}/PrivateHeaders; \
		ln -sf Versions/Current/Headers $${framedir}/Headers; \
	done

	set -e; for i in $(FRAMEWORKS); do \
		l=`echo $${i} | sed -e 's/liberty/libiberty/;' -e 's/binutils/\./;' -e 's/gdb/\./;'`; \
		(cd $(OBJROOT)/$(firstword $(NATIVE_TARGETS))/$${l}/$${i}.framework/Versions/A \
		 && $(TAR) --exclude=CVS -cf - Headers) \
		| \
		(cd $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A \
		 && $(TAR) -xf -); \
	done

	set -e; for i in gdb; do \
		l=`echo $${i} | sed -e 's/liberty/libiberty/;' -e 's/binutils/\./;' -e 's/gdb/\./;'`; \
		rm -rf $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/Headers/machine; \
		mkdir -p $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/Headers/machine; \
		for j in $(NATIVE_TARGETS); do \
			mkdir -p $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/Headers/machine/$${j}; \
			(cd $(OBJROOT)/$${j}/$${l}/$${i}.framework/Versions/A/Headers/machine \
			 && $(TAR) --exclude=CVS -cf - *) \
			| \
			(cd $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/Headers/machine/$${j} \
			 && $(TAR) -xf -) \
		done; \
		for h in $(TEMPLATE_HEADERS); do \
			hg=`echo $${h} | sed -e 's/\.h//' -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'`; \
			rm -f $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/Headers/$${h}; \
			ln -s machine/$${h} $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/Headers/$${h}; \
			cat template.h | sed -e "s/@file@/$${h}/g" -e "s/@FILEGUARD@/_CONFIG_$${hg}_H_/" \
				> $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/Headers/machine/$${h}; \
		done; \
	done

install-frameworks-resources:

	$(INSTALL) -c -d $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)

	set -e;	for i in $(GDB_FRAMEWORKS); do \
		framedir=$(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework; \
		$(INSTALL) -c -d $${framedir}/Versions/A/Resources; \
		ln -sf Versions/Current/Resources $${framedir}/Resources; \
		$(INSTALL) -c -d $${framedir}/Versions/A/Resources/English.lproj; \
		cat $(SRCROOT)/Info-macos.template | sed -e "s/@NAME@/$$i/g" -e 's/@VERSION@/$(GDB_RC_VERSION)/g' > \
			$${framedir}/Versions/A/Resources/Info-macos.plist; \
	done
	set -e;	for i in $(BINUTILS_FRAMEWORKS); do \
		framedir=$(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework; \
		$(INSTALL) -c -d $${framedir}/Versions/A/Resources; \
		ln -sf Versions/Current/Resources $${framedir}/Resources; \
		$(INSTALL) -c -d $${framedir}/Versions/A/Resources/English.lproj; \
		cat $(SRCROOT)/Info-macos.template | sed -e "s/@NAME@/$$i/g" -e 's/@VERSION@/$(BINUTILS_RC_VERSION)/g' > \
			$${framedir}/Versions/A/Resources/Info-macos.plist; \
	done

install-frameworks-macosx:

	$(SUBMAKE) CURRENT_ROOT=$(SYMROOT) install-frameworks-headers
	$(SUBMAKE) CURRENT_ROOT=$(DSTROOT) install-frameworks-headers
	$(SUBMAKE) CURRENT_ROOT=$(SYMROOT) install-frameworks-resources
	$(SUBMAKE) CURRENT_ROOT=$(DSTROOT) install-frameworks-resources

ifeq ($(CONFIG_ENABLE_SHARED),--enable-shared)
	set -e; for i in $(FRAMEWORKS); do \
		j=`echo $${i} | sed -e 's/liberty/libiberty/;' -e 's/binutils/\./;' -e 's/gdb/\./;'`; \
		lipo -create -output $(SYMROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/$${i} \
			$(patsubst %,$(OBJROOT)/%/$${j}/$${i}.framework/Versions/A/$${i},$(NATIVE_TARGETS)); \
		strip -S -o $(DSTROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/$${i} \
			 $(SYMROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/$${i}; \
		ln -sf Versions/Current/$${i} $(SYMROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/$${i}; \
		ln -sf Versions/Current/$${i} $(DSTROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/$${i}; \
	done
endif

# We no longer install things in /usr/lib -- jmolenda 2004-06-16
#
#	$(INSTALL) -c -d $(SYMROOT)/$(LIB_DIR)
#	$(INSTALL) -c -d $(DSTROOT)/$(LIB_DIR)

ifeq ($(CONFIG_ENABLE_SHARED),--enable-shared)
	lipo -create -output $(SYMROOT)/$(LIB_DIR)/libintl.a \
		$(patsubst %,$(OBJROOT)/%/intl/.libs/libintl.a,$(NATIVE_TARGETS))
	strip -S -o $(DSTROOT)/$(LIB_DIR)/libintl.a \
		 $(SYMROOT)/$(LIB_DIR)/libintl.a
	lipo -create -output $(SYMROOT)/$(LIB_DIR)/libintl.1.0.0.dylib \
		$(patsubst %,$(OBJROOT)/%/intl/.libs/libintl.1.0.0.dylib,$(NATIVE_TARGETS))
	strip -S -o $(DSTROOT)/$(LIB_DIR)/libintl.1.0.0.dylib \
		 $(SYMROOT)/$(LIB_DIR)/libintl.1.0.0.dylib
	ln -sf libintl.1.0.0.dylib $(DSTROOT)/$(LIB_DIR)/libintl.1.dylib
	ln -sf libintl.1.0.0.dylib $(SYMROOT)/$(LIB_DIR)/libintl.1.dylib
	ln -sf libintl.1.0.0.dylib $(DSTROOT)/$(LIB_DIR)/libintl.dylib
	ln -sf libintl.1.0.0.dylib $(SYMROOT)/$(LIB_DIR)/libintl.dylib
endif

install-gdb-common:

	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
		\
		$(INSTALL) -c -d $${dstroot}/$(DEVEXEC_DIR); \
		$(INSTALL) -c -d $${dstroot}/$(CONFIG_DIR); \
		$(INSTALL) -c -d $${dstroot}/$(CONF_DIR); \
		$(INSTALL) -c -d $${dstroot}/$(MAN_DIR); \
		\
		docroot="$${dstroot}/$(SYSTEM_DEVELOPER_TOOLS_DOC_DIR)/gdb"; \
		\
		$(INSTALL) -c -d "$${docroot}"; \
		\
		$(INSTALL) -c -m 644 $(SRCROOT)/doc/refcard.pdf "$${docroot}/refcard.pdf"; \
		\
	done;

install-gdb-macosx-common: install-gdb-common

	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
		\
		$(INSTALL) -c -d $${dstroot}/$(LIBEXEC_GDB_DIR); \
		\
		docroot="$${dstroot}/$(SYSTEM_DEVELOPER_TOOLS_DOC_DIR)/gdb"; \
		\
		for i in gdb gdbint stabs; do \
			$(INSTALL) -c -d "$${docroot}/$${i}"; \
			(cd "$${docroot}/$${i}" && \
				$(SRCROOT)/texi2html \
					-split_chapter \
					-I$(OBJROOT)/$(firstword $(NATIVE_TARGETS))/gdb/doc \
					-I$(SRCROOT)/src/readline/doc \
					-I$(SRCROOT)/src/gdb/mi \
					$(SRCROOT)/src/gdb/doc/$${i}.texinfo); \
		done; \
		\
		$(INSTALL) -c -d $${dstroot}/$(MAN_DIR)/man1; \
		$(INSTALL) -c -m 644 $(SRCROOT)/src/gdb/gdb.1 $${dstroot}/$(MAN_DIR)/man1/gdb.1; \
		perl -pi -e 's,GDB_DOCUMENTATION_DIRECTORY,$(SYSTEM_DEVELOPER_TOOLS_DOC_DIR)/gdb,' $${dstroot}/$(MAN_DIR)/man1/gdb.1; \
		\
		$(INSTALL) -c -d $${dstroot}/$(CONFIG_DIR); \
		$(INSTALL) -c -m 644 $(SRCROOT)/gdb.conf $${dstroot}/$(CONFIG_DIR)/gdb.conf; \
		\
		$(INSTALL) -c -d $${dstroot}/$(CONF_DIR); \
		for j in $(SRCROOT)/conf/*.gdb; do \
			$(INSTALL) -c -m 644 $$j $${dstroot}/$(CONF_DIR)/; \
		done; \
		\
		sed -e 's/version=.*/version=$(GDB_VERSION)-$(GDB_RC_VERSION)/' \
			< $(SRCROOT)/gdb.sh > $${dstroot}/usr/bin/gdb; \
		chmod 755 $${dstroot}/usr/bin/gdb; \
		\
	done;

install-gdb-macosx: install-gdb-macosx-common

	set -e; for target in $(CANONICAL_ARCHS); do \
		lipo -create $(OBJROOT)/$${target}--$${target}/gdb/gdb \
			-output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
	 	strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} \
			$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
	done

install-gdb-fat: install-gdb-macosx-common

	lipo -create $(patsubst %,$(OBJROOT)/%/gdb/gdb,$(PPC_TARGET)--$(PPC_TARGET) $(I386_TARGET)--$(PPC_TARGET)) \
		-output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$(PPC_TARGET)
	lipo -create $(patsubst %,$(OBJROOT)/%/gdb/gdb,$(PPC_TARGET)--$(I386_TARGET) $(I386_TARGET)--$(I386_TARGET)) \
		-output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$(I386_TARGET)

	set -e; for target in $(CANONICAL_ARCHS); do \
	 	strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} \
			$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
		if echo $${target} | egrep '^[^-]*-apple-darwin' > /dev/null; then \
			echo "stripping __objcInit"; \
			echo "__objcInit" > /tmp/macosx-syms-to-remove; \
			strip -R /tmp/macosx-syms-to-remove -X $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} || true; \
			rm -f /tmp/macosx-syms-to-remove; \
		fi; \
	done

install-binutils-macosx:

# We no longer install anything in /usr/libexec/binutils -- jmolenda 2004-06-16
#
#	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
#		\
#		$(INSTALL) -c -d $${dstroot}/$(LIBEXEC_BINUTILS_DIR); \
#	done
#
	set -e; for i in $(BINUTILS_BINARIES); do \
		instname=`echo $${i} | sed -e 's/\\-new//'`; \
		lipo -create $(patsubst %,$(OBJROOT)/%/binutils/$${i},$(NATIVE_TARGETS)) \
			-output $(SYMROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname}; \
		strip -S -o $(DSTROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname} $(SYMROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname}; \
	done

# "procmod" is a new group (2005-09-27) which will not be present on all
# the systems, so we use a '-' prefix on that loop for now so the errors
# don't halt the build. 

install-chmod-macosx:
	set -e;	if [ `whoami` = 'root' ]; then \
		for dstroot in $(SYMROOT) $(DSTROOT); do \
			chown -R root:wheel $${dstroot}; \
			chmod -R  u=rwX,g=rX,o=rX $${dstroot}; \
			chmod a+x $${dstroot}/$(LIBEXEC_GDB_DIR)/*; \
			chmod a+x $${dstroot}/$(DEVEXEC_DIR)/*; \
		done; \
	fi
	-set -e; if [ `whoami` = 'root' ]; then \
		for dstroot in $(SYMROOT) $(DSTROOT); do \
			chgrp procmod $${dstroot}/$(LIBEXEC_GDB_DIR)/gdb* && chmod g+s $${dstroot}/$(LIBEXEC_GDB_DIR)/gdb*; \
		done; \
	fi
ifeq ($(CONFIG_ENABLE_SHARED),--enable-shared)
	set -e;	if [ `whoami` = 'root' ]; then \
		for dstroot in $(SYMROOT) $(DSTROOT); do \
			for i in $(FRAMEWORKS); do \
				chmod a+x $${dstroot}/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/$${i}; \
			done; \
		done; \
	fi
endif

install-source:
	$(INSTALL) -c -d $(DSTROOT)/$(SOURCE_DIR)
	$(TAR) --exclude=CVS -C $(SRCROOT) -cf - . | $(TAR) -C $(DSTROOT)/$(SOURCE_DIR) -xf -

all: build

clean:
	$(RM) -r $(OBJROOT)

check-args:
ifeq "$(CANONICAL_ARCHS)" "i386-apple-darwin"
else
ifeq "$(CANONICAL_ARCHS)" "powerpc-apple-darwin"
else
ifeq "$(CANONICAL_ARCHS)" "i386-apple-darwin powerpc-apple-darwin"
else
ifeq "$(CANONICAL_ARCHS)" "powerpc-apple-darwin i386-apple-darwin"
else
	echo "Unknown architecture string: \"$(CANONICAL_ARCHS)\""
	exit 1
endif
endif
endif
endif

configure: 
ifneq ($(findstring darwin,$(CANONICAL_ARCHS)),)
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-rc-configure, $(NATIVE_TARGETS))
endif
ifneq ($(CROSS_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-rc-configure-cross, $(CROSS_TARGETS))
endif
endif

build-headers:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-headers, $(NATIVE_TARGETS)) 
endif
ifneq ($(CROSS_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-headers, $(CROSS_TARGETS))
endif

build-core:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-core, $(NATIVE_TARGETS)) 
endif
ifneq ($(CROSS_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-core, $(CROSS_TARGETS))
endif

build-binutils:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-binutils, $(NATIVE_TARGETS))
endif

build-gdb:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb, $(NATIVE_TARGETS))
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb-framework, $(NATIVE_TARGETS))
endif
ifneq ($(CROSS_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb, $(CROSS_TARGETS))
endif

build-gdb-docs:
	$(MAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(MAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb-docs, $(NATIVE_TARGETS))
endif
ifneq ($(CROSS_TARGETS),)
	$(MAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb-docs, $(CROSS_TARGETS))
endif

build:
	$(SUBMAKE) check-args
	$(SUBMAKE) build-core
	$(SUBMAKE) build-binutils
	$(SUBMAKE) build-gdb 
	$(SUBMAKE) build-gdb-docs 

install-clean:
	$(RM) -r $(SYMROOT) $(DSTROOT)

install-macosx:
	$(SUBMAKE) install-clean
ifeq "$(CANONICAL_ARCHS)" "i386-apple-darwin powerpc-apple-darwin"
	$(SUBMAKE) install-frameworks-macosx NATIVE_TARGET=unknown--unknown
	$(SUBMAKE) install-binutils-macosx NATIVE_TARGET=unknown--unknown
	$(SUBMAKE) install-gdb-fat NATIVE_TARGET=unknown--unknown PPC_TARGET=powerpc-apple-darwin I386_TARGET=i386-apple-darwin
	$(SUBMAKE) install-macsbug
else
	$(SUBMAKE) install-frameworks-macosx
	$(SUBMAKE) install-binutils-macosx
	$(SUBMAKE) install-gdb-macosx
ifeq "$(CANONICAL_ARCHS)" "powerpc-apple-darwin"
	$(SUBMAKE) install-macsbug
endif
endif
	$(SUBMAKE) install-chmod-macosx

install-macsbug:
	$(SUBMAKE) -C $(SRCROOT)/macsbug GDB_BUILD_ROOT=$(DSTROOT) BINUTILS_BUILD_ROOT=$(DSTROOT) SRCROOT=$(SRCROOT)/macsbug OBJROOT=$(OBJROOT)/powerpc-apple-darwin--powerpc-apple-darwin/macsbug SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) install
 
install:
	$(SUBMAKE) check-args
	$(SUBMAKE) build
	$(SUBMAKE) $(MACOSX_FLAGS) install-macosx
	$(SUBMAKE) $(MACOSX_FLAGS) install-chmod-macosx

installhdrs:
	$(SUBMAKE) check-args
	$(SUBMAKE) configure 
	$(SUBMAKE) build-headers
	$(SUBMAKE) install-clean
	$(SUBMAKE) $(MACOSX_FLAGS) CURRENT_ROOT=$(SYMROOT) install-frameworks-headers
	$(SUBMAKE) $(MACOSX_FLAGS) CURRENT_ROOT=$(DSTROOT) install-frameworks-headers

installsrc:
	$(SUBMAKE) check
	$(TAR) --dereference --exclude=CVS --exclude=src/contrib --exclude=src/dejagnu --exclude=src/etc --exclude=src/expect --exclude=src/sim --exclude=src/tcl --exclude=src/texinfo --exclude=src/utils -cf - . | $(TAR) -C $(SRCROOT) -xf -



check:
	[ -z `find . -name \*~ -o -name .\#\*` ] || \
		(echo 'Emacs or CVS backup files present; not copying.' && exit 1)
