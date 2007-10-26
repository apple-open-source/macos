GDB_VERSION = 6.1-20040303
GDB_RC_VERSION = 443

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
CC = cc -arch $(HOST_ARCHITECTURE)
CC_FOR_BUILD = NEXT_ROOT= cc
CDEBUGFLAGS = -g -Os -mdynamic-no-pic
CFLAGS = $(strip $(RC_CFLAGS_NOARCH) $(CDEBUGFLAGS) -Wall -Wimplicit -Wno-long-double)
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

FRAMEWORK_TARGET=stamp-framework-headers all
framework=-L../$(patsubst liberty,libiberty,$(1)) -l$(patsubst liberty,iberty,$(1))

FSFLAGS = \
	$(SFLAGS)

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

$(OBJROOT)/%/stamp-build-gdb:
	$(SUBMAKE) -C $(OBJROOT)/$* configure-gdb
	$(SUBMAKE) -C $(OBJROOT)/$*/gdb -W version.in $(MFLAGS) $(FSFLAGS) VERSION='$(GDB_VERSION_STRING)' gdb

TEMPLATE_HEADERS = config.h tm.h xm.h nm.h

install-gdb-common:

	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
                $(INSTALL) -c -d $${dstroot}/$(LIBEXEC_GDB_DIR); \
	done;

install-gdb-thin: install-gdb-common

	set -e; for target in $(CANONICAL_ARCHS); do \
		lipo -create $(OBJROOT)/$${target}--$${target}/gdb/gdb \
			-output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-for-codewarrior; \
	 	strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-for-codewarror \
			$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-for-codewarrior; \
	done

install-gdb-fat: install-gdb-common

	lipo -create $(patsubst %,$(OBJROOT)/%/gdb/gdb,$(PPC_TARGET)--$(PPC_TARGET) $(I386_TARGET)--$(I386_TARGET)) \
		-output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-for-codewarrior

	set -e; for target in $(CANONICAL_ARCHS); do \
	 	strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-for-codewarrior \
			$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-for-codewarrior; \
		if echo $${target} | egrep '^[^-]*-apple-darwin' > /dev/null; then \
			echo "stripping __objcInit"; \
			echo "__objcInit" > /tmp/macosx-syms-to-remove; \
			strip -R /tmp/macosx-syms-to-remove -X $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-for-codewarrior || true; \
			rm -f /tmp/macosx-syms-to-remove; \
		fi; \
	done

# "procmod" is a new group (2005-09-27) which will not be present on all
# the systems, so we use a '-' prefix on that loop for now so the errors
# don't halt the build. 

install-chmod-macosx:
	set -e;	for dstroot in $(SYMROOT) $(DSTROOT); do \
			chown -R root:wheel $${dstroot}; \
			chmod -R  u=rwX,g=rX,o=rX $${dstroot}; \
			chmod a+x $${dstroot}/$(LIBEXEC_GDB_DIR)/*; \
		done
	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
			chgrp procmod $${dstroot}/$(LIBEXEC_GDB_DIR)/gdb* && chmod g+s $${dstroot}/$(LIBEXEC_GDB_DIR)/gdb*; \
		done

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
endif

build-headers:

build-core:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-core, $(NATIVE_TARGETS)) 
endif

build-binutils:

build-gdb:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb, $(NATIVE_TARGETS))
endif

build-gdb-docs:

build:
	$(SUBMAKE) check-args
	$(SUBMAKE) build-core
	$(SUBMAKE) build-gdb 

install-clean:
	$(RM) -r $(SYMROOT) $(DSTROOT)

install-macosx:
	$(SUBMAKE) install-clean
ifeq "$(CANONICAL_ARCHS)" "i386-apple-darwin powerpc-apple-darwin"
	$(SUBMAKE) install-gdb-fat NATIVE_TARGET=unknown--unknown PPC_TARGET=powerpc-apple-darwin I386_TARGET=i386-apple-darwin
else
	$(SUBMAKE) install-gdb-thin
endif
	$(SUBMAKE) install-chmod-macosx

install-macsbug:
 
install:
	$(SUBMAKE) check-args
	$(SUBMAKE) build
	$(SUBMAKE) $(MACOSX_FLAGS) install-macosx
	$(SUBMAKE) $(MACOSX_FLAGS) install-chmod-macosx

installhdrs:

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

