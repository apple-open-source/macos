GDB_VERSION = 6.3.50-20050815
GDB_RC_VERSION = 1344

BINUTILS_VERSION = 2.13-20021117
BINUTILS_RC_VERSION = 46

.PHONY: all clean configure build install installsrc installhdrs headers \
	build-core build-binutils build-gdb \
	install-frameworks-headers\
	install-frameworks-macosx \
	install-binutils-macosx \
	install-gdb-fat \
	install-chmod-macosx install-chmod-macosx-noprocmod \
	install-clean install-source check


# Get the correct setting for SYSTEM_DEVELOPER_TOOLS_DOC_DIR if 
# the platform-variables.make file exists.

OS=MACOS
SYSTEM_DEVELOPER_TOOLS_DOC_DIR=/Developer/Documentation/DocSets/com.apple.ADC_Reference_Library.DeveloperTools.docset/Contents/Resources/Documents/documentation/DeveloperTools


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
CANONICAL_ARCHS := $(subst macos:x86_64:$(RC_RELEASE),x86_64-apple-darwin,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst macos:ppc:$(RC_RELEASE),powerpc-apple-darwin,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst macos:arm:$(RC_RELEASE),arm-apple-darwin,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst macos:armv6:$(RC_RELEASE),arm-apple-darwin,$(CANONICAL_ARCHS))

CANONICAL_ARCHS := $(subst powerpc-apple-darwin i386-apple-darwin,i386-apple-darwin powerpc-apple-darwin,$(CANONICAL_ARCHS))

CANONICAL_ARCHS := $(sort $(CANONICAL_ARCHS))

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
ifeq (arm,$(ARCH_SAYS))
BUILD_ARCH := arm-apple-darwin
else
BUILD_ARCH := $(ARCH_SAYS)
endif
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
BINUTILS_BUILD_ROOT = $(SDKROOT)
endif

BINUTILS_FRAMEWORK_PATH = $(BINUTILS_BUILD_ROOT)/System/Library/PrivateFrameworks
BINUTILS_LIB_PATH = $(BINUTILS_BUILD_ROOT)/usr/lib

BFD_FRAMEWORK = $(BINUTILS_FRAMEWORK_PATH)/bfd.framework
BFD_HEADERS = $(BFD_FRAMEWORK)/Headers

LIBERTY_FRAMEWORK = $(BINUTILS_FRAMEWORK_PATH)/liberty.framework
LIBERTY_HEADERS = $(LIBERTY_FRAMEWORK)/Headers

OPCODES_FRAMEWORK = $(BINUTILS_FRAMEWORK_PATH)/opcodes.framework
OPCODES_HEADERS = $(OPCODES_FRAMEWORK)/Headers

BINUTILS_FRAMEWORK = $(BINUTILS_FRAMEWORK_PATH)/binutils.framework
BINUTILS_HEADERS = $(BINUTILS_FRAMEWORK)/Headers

INTL_FRAMEWORK = $(BINUTILS_BUILD_ROOT)/usr/lib/libintl.dylib
INTL_HEADERS = $(BINUTILS_BUILD_ROOT)/usr/include

TAR = gnutar
CPP = cpp
CC = gcc
CXX = c++
LD = ld
AR = ar
RANLIB = ranlib
NM = nm
CC_FOR_BUILD = gcc

ifndef CDEBUGFLAGS
CDEBUGFLAGS = -g -Os -funwind-tables -fasynchronous-unwind-tables -D_DARWIN_UNLIMITED_STREAMS
endif

CFLAGS = $(CDEBUGFLAGS) $(RC_CFLAGS)
HOST_ARCHITECTURE = UNKNOWN

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

NATIVE_TARGETS = $(foreach arch1,$(CANONICAL_ARCHS),$(arch1)--$(arch1))

CROSS_TARGETS = $(strip $(foreach hostarch, $(CANONICAL_ARCHS), $(foreach targarch, $(filter-out $(hostarch), $(CANONICAL_ARCHS)), $(hostarch)--$(targarch))))
CROSS_TARGETS := $(filter-out x86_64-apple-darwin--i386-apple-darwin, $(CROSS_TARGETS))
CROSS_TARGETS := $(filter-out i386-apple-darwin--x86_64-apple-darwin, $(CROSS_TARGETS))
CROSS_TARGETS := $(filter-out powerpc-apple-darwin--x86_64-apple-darwin, $(CROSS_TARGETS))
CROSS_TARGETS := $(filter-out arm-apple-darwin--x86_64-apple-darwin, $(CROSS_TARGETS))
CROSS_TARGETS := $(sort $(CROSS_TARGETS))

CONFIG_VERBOSE=-v
CONFIG_ENABLE_GDBTK=--enable-gdbtk=no
CONFIG_ENABLE_GDBMI=
CONFIG_ENABLE_BUILD_WARNINGS=--enable-build-warnings
CONFIG_ENABLE_TUI=--disable-tui
CONFIG_ALL_BFD_TARGETS=
CONFIG_ALL_BFD_TARGETS=
CONFIG_64_BIT_BFD=--enable-64-bit-bfd
CONFIG_WITH_MMAP=--with-mmap
CONFIG_ENABLE_SHARED=--disable-shared
CONFIG_MAINTAINER_MODE=
CONFIG_BUILD=--build=$(BUILD_ARCH)
CONFIG_OTHER_OPTIONS?=--disable-serial-configure

# The code below looks like it is intended for building the ARM native debugger.
# When this is done in B&I we get passed in these settings,
#    RC_CFLAGS:                      -arch armv6 -pipe
#    RC_ARCHS:                       armv6
#    RC_OS:                          macos
#    RC_RELEASE                       "BigBear"
#    RC_PRODUCT                       "P2"
#    RC_PURPLE                        "YES"
#    RC_armv6                         "YES"
#    RC_arm                           ""
#    TRAIN                            "BigBear"
#    HOST_ARCHITECTURE=armv6
#    SDKROOT='/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.Internal.sdk'
#
ifneq ($(findstring macosx,$(CANONICAL_ARCHS))$(findstring darwin,$(CANONICAL_ARCHS)),)
CC = gcc -arch $(HOST_ARCHITECTURE)
CC_FOR_BUILD = gcc

# Unset this when building Canadian Cross.  (e.g. an arm native gdb built on an
# i386 system)  This will have a setting like "10.5" which is not valid on the
# iPhone OS platform and our compiler will get linker errors when running
# autoconf tests.
MACOSX_DEPLOYMENT_TARGET=
CDEBUGFLAGS = -g -Os

# The -Wno-error=deprecated-declarations flag is not recognized by some 
# compilers; disable it on a per-release basis.

ifeq (Leopard,$(RC_RELEASE))
OS_DEP_CFLAGS = 
else
ifeq (SnowLeopard,$(RC_RELEASE))
OS_DEP_CFLAGS = -Wno-error=deprecated-declarations
else
OS_DEP_CFLAGS = 
endif
endif

CFLAGS = $(strip $(RC_NONARCH_CFLAGS) $(CDEBUGFLAGS) -Wall -Wimplicit $(OS_DEP_CFLAGS) -Werror-implicit-function-declaration -funwind-tables -fasynchronous-unwind-tables)
HOST_ARCHITECTURE = $(shell echo $* | sed -e 's/--.*//' -e 's/powerpc/ppc/' -e 's/-apple-macosx.*//' -e 's/-apple-macos.*//' -e 's/-apple-darwin.*//')
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
	SDKROOT='$(SDKROOT)' \
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

FRAMEWORK_TARGET=stamp-framework-headers all
framework=-L../$(patsubst liberty,libiberty,$(1)) -l$(patsubst liberty,iberty,$(1))

FSFLAGS = $(SFLAGS)

CONFIGURE_ENV = $(EFLAGS)
MAKE_ENV = $(EFLAGS)

SUBMAKE = $(MAKE_ENV) $(MAKE)

_all: all


crossarm: LIBEXEC_GDB_DIR=usr/libexec/gdb
crossarm: CDEBUGFLAGS ?= -gdwarf-2 -D__DARWIN_UNIX03=0


crossarm:;
        
	echo BUILDING CROSSARM; \
	$(RM)  $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-arm-apple-darwin
	for i in $(RC_ARCHS); do \
		$(RM) -r $(OBJROOT)/$${i}-apple-darwin--arm-apple-darwin; \
		$(INSTALL) -c -d $(OBJROOT)/$${i}-apple-darwin--arm-apple-darwin; \
		(cd $(OBJROOT)/$${i}-apple-darwin--arm-apple-darwin/ && \
			$(CONFIGURE_ENV) CC="cc -arch $${i}" $(MACOSX_FLAGS) $(SRCTOP)/src/configure \
				--host=$${i}-apple-darwin \
				--target=arm-apple-darwin \
                                --build=$(BUILD_ARCH) \
				CFLAGS=" -isystem $(SDKROOT)/usr/include $(CDEBUGFLAGS)" \
				$(CONFIGURE_OPTIONS) \
			); \
		mkdir -p $(SYMROOT)/$(LIBEXEC_GDB_DIR); \
		mkdir -p $(DSTROOT)/$(LIBEXEC_GDB_DIR); \
		$(SUBMAKE) $(MACOSX_FLAGS) -C $(OBJROOT)/$${i}-apple-darwin--arm-apple-darwin configure-gdb; \
		$(SUBMAKE) $(MACOSX_FLAGS) -C $(OBJROOT)/$${i}-apple-darwin--arm-apple-darwin \
				-W version.in VERSION='$(GDB_VERSION_STRING)' GDB_RC_VERSION='$(GDB_RC_VERSION)' all-gdb; \
		if [ -e $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-arm-apple-darwin ] ; then \
			lipo -create $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-arm-apple-darwin $(OBJROOT)/$${i}-apple-darwin--arm-apple-darwin/gdb/gdb -output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-arm-apple-darwin; \
		else \
			lipo -create $(OBJROOT)/$${i}-apple-darwin--arm-apple-darwin/gdb/gdb -output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-arm-apple-darwin; \
		fi;\
	done;
	(cd $(SYMROOT)/$(LIBEXEC_GDB_DIR)/ ; dsymutil gdb-arm-apple-darwin)
	strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-arm-apple-darwin \
		$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-arm-apple-darwin
	chown root:wheel $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-arm-apple-darwin
	chmod 755 $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-arm-apple-darwin
	mkdir -p ${DSTROOT}/usr/bin
	sed -e 's/version=.*/version=$(GDB_VERSION)-$(GDB_RC_VERSION)/' \
                < $(SRCROOT)/gdb.sh > ${DSTROOT}/usr/bin/gdb
	chmod 755 ${DSTROOT}/usr/bin/gdb

#
# cross
# 
# Build only a cross targets for host architectures. RC_ARCHS specifies
# all of the host architectures to build for, and RC_CROSS_ARCHS specifies
# all of the cross architectures to build for. This can save time when
# building a cross target as you don't have to build all permutations of
# RC_ARCHS. 
#
# The command below will build a cross i386 gdb to be hosted on ppc:
# sudo ~rc/bin/buildit -noinstallhdrs -arch ppc -target cross /path/to/gdb RC_CROSS_ARCHS=i386
#


cross: 	LIBEXEC_GDB_DIR=usr/libexec/gdb
cross:;
	echo BUILDING CROSS $(RC_CROSS_ARCHS) for HOST $(RC_ARCHS); \
        for cross_arch in $(RC_CROSS_ARCHS); do \
		$(RM)  $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${cross_arch}-apple-darwin; \
		for host_arch in $(RC_ARCHS); do \
			echo BUILDING CROSS $${cross_arch} for HOST $${host_arch}; \
			$(RM) -r $(OBJROOT)/$${host_arch}-apple-darwin--$${cross_arch}-apple-darwin; \
			$(INSTALL) -c -d $(OBJROOT)/$${host_arch}-apple-darwin--$${cross_arch}-apple-darwin; \
			(cd $(OBJROOT)/$${host_arch}-apple-darwin--$${cross_arch}-apple-darwin/ && \
				$(CONFIGURE_ENV) CC="cc -arch $${host_arch}" $(MACOSX_FLAGS) $(SRCTOP)/src/configure \
					--host=$${host_arch}-apple-darwin \
					--target=$${cross_arch}-apple-darwin \
					--build=$(BUILD_ARCH) \
					CFLAGS=" -isystem $(SDKROOT)/usr/include $(CDEBUGFLAGS)" \
					$(CONFIGURE_OPTIONS) \
				); \
			mkdir -p $(SYMROOT)/$(LIBEXEC_GDB_DIR); \
			mkdir -p $(DSTROOT)/$(LIBEXEC_GDB_DIR); \
			$(SUBMAKE) $(MACOSX_FLAGS) -C $(OBJROOT)/$${host_arch}-apple-darwin--$${cross_arch}-apple-darwin configure-gdb; \
			$(SUBMAKE) $(MACOSX_FLAGS) -C $(OBJROOT)/$${host_arch}-apple-darwin--$${cross_arch}-apple-darwin \
				-W version.in VERSION='$(GDB_VERSION_STRING)' GDB_RC_VERSION='$(GDB_RC_VERSION)' all-gdb; \
			if [ -e $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${cross_arch}-apple-darwin ] ; then \
				lipo -create $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${cross_arch}-apple-darwin $(OBJROOT)/$${host_arch}-apple-darwin--$${cross_arch}-apple-darwin/gdb/gdb -output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${cross_arch}-apple-darwin; \
			else \
				lipo -create $(OBJROOT)/$${host_arch}-apple-darwin--$${cross_arch}-apple-darwin/gdb/gdb -output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${cross_arch}-apple-darwin; \
			fi; \
		done; \
                (cd $(SYMROOT)/$(LIBEXEC_GDB_DIR)/ ; dsymutil gdb-$${cross_arch}-apple-darwin); \
		strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${cross_arch}-apple-darwin \
			$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${cross_arch}-apple-darwin; \
	done; \
	mkdir -p ${DSTROOT}/usr/bin; \
	sed -e 's/version=.*/version=$(GDB_VERSION)-$(GDB_RC_VERSION)/' \
                < $(SRCROOT)/gdb.sh > ${DSTROOT}/usr/bin/gdb; \
	chmod 755 ${DSTROOT}/usr/bin/gdb; \

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
	$(SUBMAKE) -C $(OBJROOT)/$* configure-intl configure-libiberty configure-bfd configure-opcodes configure-gdb
	$(SUBMAKE) -C $(OBJROOT)/$*/libiberty $(FFLAGS) stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$*/bfd $(FFLAGS) headers stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$*/opcodes $(FFLAGS) stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-headers-binutils
	$(SUBMAKE) -C $(OBJROOT)/$*/gdb/doc $(MFLAGS) VERSION='$(GDB_VERSION_STRING)'
	$(SUBMAKE) -C $(OBJROOT)/$* $(MFLAGS) stamp-framework-headers-gdb
	#touch $@

$(OBJROOT)/%/stamp-build-core:
	$(SUBMAKE) -C $(OBJROOT)/$* configure-intl configure-libiberty configure-bfd configure-opcodes
	$(SUBMAKE) -C $(OBJROOT)/$*/intl $(SFLAGS) libintl.la
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
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-headers-binutils
	#touch $@

$(OBJROOT)/%/stamp-build-gdb:
	$(SUBMAKE) -C $(OBJROOT)/$* configure-gdb
	$(SUBMAKE) -C $(OBJROOT)/$*/gdb -W version.in $(MFLAGS) $(FSFLAGS) VERSION='$(GDB_VERSION_STRING)' GDB_RC_VERSION='$(GDB_RC_VERSION)' gdb

$(OBJROOT)/%/stamp-build-gdb-framework:
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-headers-gdb

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
		$(INSTALL) -c -d $${dstroot}/usr/local/OpenSourceLicenses; \
		$(INSTALL) -c -d $${dstroot}/usr/local/OpenSourceVersions; \
		$(INSTALL) -c -m 644 $(SRCROOT)/gdb.plist $${dstroot}/usr/local/OpenSourceVersions; \
		$(INSTALL) -c -m 644 $(SRCROOT)/gdb.txt $${dstroot}/usr/local/OpenSourceLicenses; \
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

	set -e; for target in $(filter-out x86_64-apple-darwin, $(CANONICAL_ARCHS)); do \
		lipo -create $(OBJROOT)/$${target}--$${target}/gdb/gdb \
			-output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
		dsymutil -o $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}.dSYM \
                         $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
	 	strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} \
			$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
		cp $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
	done

# When this target is invoked, NATIVE is the binary that we'll be outputting and
# HOSTCOMBOS are the binaries that will be combined into that.  For instance,
#
# HOSTCOMBOS == i386-apple-darwin--i386-apple-darwin x86_64-apple-darwin--x86_64-apple-darwin powerpc-apple-darwin--i386-apple-darwin
# NATIVE == i386-apple-darwin

# NATIVE i386 is a special case where we add the x86_64-apple-darwin variant manually.

install-gdb-fat: install-gdb-macosx-common
	lipo -create $(patsubst %,$(OBJROOT)/%/gdb/gdb,$(HOSTCOMBOS)) \
	     -output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$(NATIVE)

dsym-and-strip-fat-gdbs:
	set -e; for target in $(filter-out x86_64-apple-darwin, $(CANONICAL_ARCHS)); do \
		dsymutil -o $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}.dSYM \
                         $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
	 	strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} \
			$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
		cp $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} \
                   $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
	done

install-binutils-macosx:

	set -e; for i in $(BINUTILS_BINARIES); do \
		instname=`echo $${i} | sed -e 's/\\-new//'`; \
		lipo -create $(patsubst %,$(OBJROOT)/%/binutils/$${i},$(NATIVE_TARGETS)) \
			-output $(SYMROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname}; \
		strip -S -o $(DSTROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname} $(SYMROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname}; \
	done

install-chmod-macosx:
	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
			chown -R root:wheel $${dstroot}; \
			chmod -R  u=rwX,g=rX,o=rX $${dstroot}; \
			chmod a+x $${dstroot}/$(LIBEXEC_GDB_DIR)/*; \
			chmod a+x $${dstroot}/$(DEVEXEC_DIR)/*; \
		done
	-set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
			chgrp procmod $${dstroot}/$(LIBEXEC_GDB_DIR)/gdb* && chmod g+s $${dstroot}/$(LIBEXEC_GDB_DIR)/gdb*; \
			chgrp procmod $${dstroot}/$(LIBEXEC_GDB_DIR)/plugins/MacsBug/MacsBug_plugin && chmod g+s $${dstroot}/$(LIBEXEC_GDB_DIR)/plugins/MacsBug/MacsBug_plugin; \
		done

install-chmod-macosx-noprocmod:
	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
			chown -R root:wheel $${dstroot}; \
			chmod -R  u=rwX,g=rX,o=rX $${dstroot}; \
			chmod a+x $${dstroot}/$(LIBEXEC_GDB_DIR)/*; \
			chmod a+x $${dstroot}/$(DEVEXEC_DIR)/*; \
			chmod 755 $${dstroot}/$(LIBEXEC_GDB_DIR)/gdb-*-apple-darwin; \
		done

install-source:
	$(INSTALL) -c -d $(DSTROOT)/$(SOURCE_DIR)
	$(TAR) --exclude=CVS -C $(SRCROOT) -cf - . | $(TAR) -C $(DSTROOT)/$(SOURCE_DIR) -xf -

all: build

clean:
	$(RM) -r $(OBJROOT)

check-args:
ifneq (,$(filter-out i386-apple-darwin, $(filter-out powerpc-apple-darwin, $(filter-out x86_64-apple-darwin, $(filter-out arm-apple-darwin, $(CANONICAL_ARCHS))))))
	echo "Unknown architecture string: \"$(CANONICAL_ARCHS)\""
	exit 1
endif

configure-headers:
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-rc-configure, $(NATIVE_TARGETS))
endif

configure: 
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-rc-configure, $(NATIVE_TARGETS))
endif
ifneq ($(CROSS_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-rc-configure-cross, $(CROSS_TARGETS))
endif

build-headers:
	$(SUBMAKE) configure-headers
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-headers, $(NATIVE_TARGETS))
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
ifneq ($(CROSS_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb, $(CROSS_TARGETS))
endif
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb, $(NATIVE_TARGETS))
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb-framework, $(NATIVE_TARGETS))
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
	$(RM) -r $(DSTROOT)

install-macosx:
	$(SUBMAKE) install-clean
	$(SUBMAKE) install-frameworks-macosx 
	$(SUBMAKE) install-binutils-macosx 
ifneq (,$(findstring i386-apple-darwin, $(CANONICAL_ARCHS)))
	$(SUBMAKE) install-gdb-fat HOSTCOMBOS="$(sort $(filter i386-apple-darwin--i386-apple-darwin, $(NATIVE_TARGETS)) $(filter %--i386-apple-darwin, $(CROSS_TARGETS)) $(filter x86_64-apple-darwin--x86_64-apple-darwin, $(NATIVE_TARGETS)) $(filter %--x86_64-apple-darwin, $(CROSS_TARGETS)))" NATIVE=i386-apple-darwin
endif
ifneq (,$(findstring powerpc-apple-darwin, $(CANONICAL_ARCHS)))
	$(SUBMAKE) install-gdb-fat HOSTCOMBOS="$(sort $(filter powerpc-apple-darwin--powerpc-apple-darwin, $(NATIVE_TARGETS)) $(filter %--powerpc-apple-darwin, $(CROSS_TARGETS)))" NATIVE=powerpc-apple-darwin
endif
ifneq (,$(findstring arm-apple-darwin, $(CANONICAL_ARCHS)))
	$(SUBMAKE) install-gdb-fat HOSTCOMBOS="$(sort $(filter arm-apple-darwin--arm-apple-darwin, $(NATIVE_TARGETS)) $(filter %--arm-apple-darwin, $(CROSS_TARGETS)))" NATIVE=arm-apple-darwin
endif
	$(SUBMAKE) dsym-and-strip-fat-gdbs
ifneq (,$(findstring powerpc-apple-darwin, $(CANONICAL_ARCHS)))
	$(SUBMAKE) install-macsbug RC_ARCHS=ppc RC_CFLAGS="-arch ppc -pipe"
endif
ifeq "$(CANONICAL_ARCHS)" "arm-apple-darwin"
	$(SUBMAKE) install-chmod-macosx-noprocmod
else
	$(SUBMAKE) install-chmod-macosx-noprocmod
endif

install-macsbug:
	$(SUBMAKE) -C $(SRCROOT)/macsbug GDB_BUILD_ROOT=$(DSTROOT) BINUTILS_BUILD_ROOT=$(DSTROOT) SRCROOT=$(SRCROOT)/macsbug OBJROOT=$(OBJROOT)/powerpc-apple-darwin--powerpc-apple-darwin/macsbug SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) install
 
install:
	$(SUBMAKE) check-args
	$(SUBMAKE) build
	$(SUBMAKE) $(MACOSX_FLAGS) install-macosx
ifeq "$(CANONICAL_ARCHS)" "arm-apple-darwin"
	$(SUBMAKE) $(MACOSX_FLAGS) install-chmod-macosx-noprocmod
else
	$(SUBMAKE) $(MACOSX_FLAGS) install-chmod-macosx-noprocmod
endif

installhdrs:
	$(SUBMAKE) check-args
	$(SUBMAKE) configure-headers
	$(SUBMAKE) build-headers
	$(SUBMAKE) install-clean
	$(SUBMAKE) $(MACOSX_FLAGS) CURRENT_ROOT=$(SYMROOT) install-frameworks-headers
	$(SUBMAKE) $(MACOSX_FLAGS) CURRENT_ROOT=$(DSTROOT) install-frameworks-headers

installsrc:
	$(SUBMAKE) check
	$(TAR) --dereference --exclude=CVS --exclude=src/contrib --exclude=src/dejagnu --exclude=src/etc --exclude=src/expect --exclude=src/sim --exclude=src/tcl --exclude=src/texinfo --exclude=src/utils -cf - . | $(TAR) -C $(SRCROOT) -xf -



check:
	@[ -z "`find . -name \*~ -o -name .\#\*`" ] || \
	   (echo; echo 'Emacs or CVS backup files present; not copying:'; \
           find . \( -name \*~ -o -name .#\* \) -print | sed 's,^[.]/,  ,'; \
           echo Suggest: ; \
           echo '    ' find . \\\( -name \\\*~ -o -name .#\\\* \\\) -exec rm -f \{\} \\\; -print ; \
           echo; \
           exit 1)
