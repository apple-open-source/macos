.PHONY: all clean configure build install installsrc installhdrs \
	headers build-core build-binutils \
	install-frameworks-pre install-frameworks-post install-frameworks-headers\
	install-frameworks-pdo install-frameworks-rhapsody \
	install-binutils-base install-binutils-pdo install-binutils-fat \
	install-chmod-rhapsody install-chmod-pdo install-clean install-source \
	check

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

CANONICAL_ARCHS := $(subst teflon:ppc:undefined,powerpc-apple-rhapsody,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst teflon:i386:undefined,i386-apple-rhapsody,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst teflon:ppc:Hera,powerpc-apple-rhapsody,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst teflon:i386:Hera,i386-apple-rhapsody,$(CANONICAL_ARCHS))

CANONICAL_ARCHS := $(subst macos:i386:$(RC_RELEASE),i386-apple-macos10,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst macos:ppc:$(RC_RELEASE),powerpc-apple-macos10,$(CANONICAL_ARCHS))

CANONICAL_ARCHS := $(subst solaris:sparc:Zeus,sparc-nextpdo-solaris2,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst hpux:hppa:Zeus,hppa1.1-nextpdo-hpux10.20,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst solaris:sparc:Hydra,sparc-nextpdo-solaris2,$(CANONICAL_ARCHS))
CANONICAL_ARCHS := $(subst hpux:hppa:Hydra,hppa2.0n-nextpdo-hpux11.0,$(CANONICAL_ARCHS))

CANONICAL_ARCHS := $(subst powerpc-apple-macos10 i386-apple-macos10,i386-apple-macos10 powerpc-apple-macos10,$(CANONICAL_ARCHS))

SRCTOP = $(shell cd $(SRCROOT) && pwd)
OBJTOP = $(shell (test -d $(OBJROOT) || $(INSTALL) -c -d $(OBJROOT)) && cd $(OBJROOT) && pwd)
SYMTOP = $(shell (test -d $(SYMROOT) || $(INSTALL) -c -d $(SYMROOT)) && cd $(SYMROOT) && pwd)
DSTTOP = $(shell (test -d $(DSTROOT) || $(INSTALL) -c -d $(DSTROOT)) && cd $(DSTROOT) && pwd)

BINUTILS_VERSION = 5.0-20001113
APPLE_VERSION = 23.1

BINUTILS_VERSION_STRING = $(BINUTILS_VERSION) (Apple version binutils-$(APPLE_VERSION))

BINUTILS_BINARIES = objdump objcopy addr2line nm-new size strings cxxfilt
BINUTILS_MANPAGES = objdump.1 objcopy.1 addr2line.1 nm.1 size.1 strings.1 cxxfilt.man

FRAMEWORKS = electric-fence mmalloc liberty bfd opcodes binutils

MMALLOC_ADDRESS = 0xb9f00000
LIBERTY_ADDRESS = 0x66b00000
OPCODES_ADDRESS = 0x66d00000
BFD_ADDRESS = 0xb5700000
ELECTRIC_FENCE_ADDRESS = 0x0

CC = cc
CC_FOR_BUILD = cc
CDEBUGFLAGS = -g
CFLAGS = $(CDEBUGFLAGS) $(RC_CFLAGS)
HOST_ARCHITECTURE = UNKNOWN

RC_CFLAGS_NOARCH = $(shell echo $(RC_CFLAGS) | sed -e 's/-arch [a-z0-9]*//g')

SYSTEM_FRAMEWORK = -framework System -lcc_dynamic
FRAMEWORK_PREFIX =
FRAMEWORK_SUFFIX =
FRAMEWORK_VERSION = A
FRAMEWORK_VERSION_SUFFIX =

DEVEXEC_DIR=UNKNOWN
DOCUMENTATION_DIR=UNKNOWN
LIBEXEC_BINUTILS_DIR=UNKNOWN
LIBEXEC_LIB_DIR=UNKNOWN
MAN_DIR=UNKNOWN
PRIVATE_FRAMEWORKS_DIR=UNKNOWN

NATIVE_TARGET = unknown--unknown

NATIVE_TARGETS = $(foreach arch1,$(CANONICAL_ARCHS),$(arch1)--$(arch1))

CROSS_TARGETS = $(foreach arch1,$(CANONICAL_ARCHS),$(foreach arch2,$(filter-out $(arch1),$(CANONICAL_ARCHS)),$(arch1)--$(arch2)))

PPC_TARGET=UNKNOWN
I386_TARGET=UNKNOWN

CONFIG_VERBOSE=-v
CONFIG_ALL_BFD_TARGETS=--enable-targets=all
CONFIG_64_BIT_BFD=--enable-64-bit-bfd
CONFIG_WITH_MMAP=--with-mmap
CONFIG_WITH_MMALLOC=--with-mmalloc
CONFIG_WITH_MMALLOC=
CONFIG_MAINTAINER_MODE=--enable-maintainer-mode
CONFIG_OTHER_OPTIONS=

MAKE_CFM=WITH_CFM=1
MAKE_PTHREADS=USE_PTHREADS=1

TAR = gnutar

ifneq ($(findstring rhapsody,$(CANONICAL_ARCHS))$(findstring macos10,$(CANONICAL_ARCHS)),)
CC = cc -arch $(HOST_ARCHITECTURE) -traditional-cpp
CC_FOR_BUILD = NEXT_ROOT= cc -traditional-cpp
CDEBUGFLAGS = -g -O3
CFLAGS = $(CDEBUGFLAGS) -Wall -Wimplicit $(RC_CFLAGS_NOARCH)
HOST_ARCHITECTURE = $(shell echo $* | sed -e 's/--.*//' -e 's/powerpc/ppc/' -e 's/-apple-rhapsody//' -e 's/-apple-macos.*//')
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

RHAPSODY_FLAGS = \
	DEVEXEC_DIR=usr/bin \
	DOCUMENTATION_DIR=Developer/Documentation/DeveloperTools \
	LIBEXEC_BINUTILS_DIR=usr/libexec/binutils \
	MAN_DIR=usr/share/man \
	PRIVATE_FRAMEWORKS_DIR=System/Library/PrivateFrameworks \
	SOURCE_DIR=System/Developer/Source/Commands/binutils

PDO_FLAGS = \
	DEVEXEC_DIR=Developer/Executables \
	DOCUMENTATION_DIR=Documentation/Developer/DeveloperTools \
	LIBEXEC_BINUTILS_DIR=Developer/Libraries/binutils \
	MAN_DIR=Local/man \
	PRIVATE_FRAMEWORKS_DIR=Library/PrivateFrameworks \
	LIBEXEC_LIB_DIR=Library/Executables \
	SOURCE_DIR=Developer/Source/binutils

CONFIGURE_OPTIONS = \
	$(CONFIG_VERBOSE) \
	$(CONFIG_ALL_BFD_TARGETS) \
	$(CONFIG_64_BIT_BFD) \
	$(CONFIG_WITH_MMAP) \
	$(CONFIG_WITH_MMALLOC) \
	$(CONFIG_OTHER_OPTIONS)

MAKE_OPTIONS = \
	$(MAKE_CFM) \
	$(MAKE_PTHREADS)

EFLAGS = \
	CFLAGS='$(CFLAGS)' \
	CC='$(CC)' \
	CC_FOR_BUILD='$(CC_FOR_BUILD)' \
	HOST_ARCHITECTURE='$(HOST_ARCHITECTURE)' \
	NEXT_ROOT='$(NEXT_ROOT)' \
	SRCROOT='$(SRCROOT)' \
	$(MAKE_OPTIONS)

SFLAGS = $(EFLAGS)

FFLAGS = \
	$(SFLAGS) \
	SYSTEM_FRAMEWORK='$(SYSTEM_FRAMEWORK)' \
	FRAMEWORK_PREFIX='$(FRAMEWORK_PREFIX)' \
	FRAMEWORK_SUFFIX='$(FRAMEWORK_SUFFIX)' \
	FRAMEWORK_VERSION_SUFFIX='$(FRAMEWORK_VERSION_SUFFIX)'

FSFLAGS = \
	$(SFLAGS) \
	MMALLOC='-F../mmalloc -framework mmalloc' \
	OPCODES='-F../opcodes -framework opcodes' \
	BFD='-F../bfd -framework bfd' \
	LIBIBERTY='-F../libiberty -framework liberty' \
	EFENCE='-F../electric-fence -framework electric-fence'

CONFIGURE_ENV = $(EFLAGS)
MAKE_ENV = $(EFLAGS)

SUBMAKE = $(MAKE_ENV) $(MAKE)

_all: all

$(OBJROOT)/%/stamp-rc-configure-pdo:
	$(RM) -r $(OBJROOT)/$*
	$(INSTALL) -c -d $(OBJROOT)/$*
	(cd $(OBJROOT)/$* && \
		$(CONFIGURE_ENV) $(SRCTOP)/src/configure \
			--host=$(shell echo $* | sed -e 's/--.*//') \
			--target=$(shell echo $* | sed -e 's/.*--//') \
			$(CONFIGURE_OPTIONS) \
			)
	touch $@

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

$(OBJROOT)/%/stamp-build-headers:
	$(SUBMAKE) -C $(OBJROOT)/$*/electric-fence $(FFLAGS) FRAMEWORK_ADDRESS=$(ELECTRIC_FENCE_ADDRESS) stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$*/mmalloc $(FFLAGS) FRAMEWORK_ADDRESS=$(MMALLOC_ADDRESS) stamp-framework-headers 
	$(SUBMAKE) -C $(OBJROOT)/$*/libiberty $(FFLAGS) FRAMEWORK_ADDRESS=$(LIBERTY_ADDRESS) stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$*/bfd $(FFLAGS) FRAMEWORK_ADDRESS=$(BFD_ADDRESS) headers stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$*/opcodes $(FFLAGS) FRAMEWORK_ADDRESS=$(OPCODES_ADDRESS) stamp-framework-headers
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) FRAMEWORK_ADDRESS=$(OPCODES_ADDRESS) stamp-framework-headers-binutils
	#touch $@

$(OBJROOT)/%/stamp-build-core:
	$(SUBMAKE) -C $(OBJROOT)/$*/intl $(SFLAGS) libintl.a
	$(SUBMAKE) -C $(OBJROOT)/$*/electric-fence $(FFLAGS) FRAMEWORK_ADDRESS=$(ELECTRIC_FENCE_ADDRESS) all stamp-framework 
	$(SUBMAKE) -C $(OBJROOT)/$*/mmalloc $(FFLAGS) FRAMEWORK_ADDRESS=$(MMALLOC_ADDRESS) all stamp-framework 
	$(SUBMAKE) -C $(OBJROOT)/$*/libiberty $(FFLAGS) FRAMEWORK_ADDRESS=$(LIBERTY_ADDRESS) all stamp-framework
	$(SUBMAKE) -C $(OBJROOT)/$*/bfd $(FFLAGS) FRAMEWORK_ADDRESS=$(BFD_ADDRESS) headers all stamp-framework
	$(SUBMAKE) -C $(OBJROOT)/$*/opcodes $(FFLAGS) FRAMEWORK_ADDRESS=$(OPCODES_ADDRESS) all stamp-framework
	#touch $@

$(OBJROOT)/%/stamp-build-binutils:
	$(SUBMAKE) -C $(OBJROOT)/$*/binutils $(FSFLAGS) VERSION='$(BINUTILS_VERSION_STRING)'
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-binutils
	#touch $@

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
		| (cd $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A \
		 && $(TAR) -xf -); \
	done

install-frameworks-resources:

	$(INSTALL) -c -d $(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)

	set -e;	for i in $(FRAMEWORKS); do \
		framedir=$(CURRENT_ROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework; \
		$(INSTALL) -c -d $${framedir}/Versions/A/Resources; \
		ln -sf Versions/Current/Resources $${framedir}/Resources; \
		$(INSTALL) -c -d $${framedir}/Versions/A/Resources/English.lproj; \
		cat $(SRCROOT)/Info-macos.template | sed -e "s/@NAME@/$$i/g" -e 's/@VERSION@/$(APPLE_VERSION)/g' > \
			$${framedir}/Versions/A/Resources/Info-macos.plist; \
	done

install-frameworks-rhapsody:

	$(SUBMAKE) CURRENT_ROOT=$(SYMROOT) install-frameworks-headers
	$(SUBMAKE) CURRENT_ROOT=$(DSTROOT) install-frameworks-headers
	$(SUBMAKE) CURRENT_ROOT=$(SYMROOT) install-frameworks-resources
	$(SUBMAKE) CURRENT_ROOT=$(DSTROOT) install-frameworks-resources

	set -e; for i in $(FRAMEWORKS); do \
		j=`echo $${i} | sed -e 's/liberty/libiberty/;' -e 's/binutils/\./;'`; \
		lipo -create -output $(SYMROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/$${i} \
			$(patsubst %,$(OBJROOT)/%/$${j}/$${i}.framework/Versions/A/$${i},$(NATIVE_TARGETS)); \
		strip -S -o $(DSTROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/$${i} \
			 $(SYMROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/$${i}; \
		ln -sf Versions/Current/$${i} $(SYMROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/$${i}; \
		ln -sf Versions/Current/$${i} $(DSTROOT)/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/$${i}; \
	done

install-frameworks-pdo:

	$(SUBMAKE) CURRENT_ROOT=$(SYMROOT) install-frameworks-headers
	$(SUBMAKE) CURRENT_ROOT=$(DSTROOT) install-frameworks-headers
	$(SUBMAKE) CURRENT_ROOT=$(SYMROOT) install-frameworks-resources
	$(SUBMAKE) CURRENT_ROOT=$(DSTROOT) install-frameworks-resources

	$(INSTALL) -c -d $(SYMROOT)/$(LIBEXEC_LIB_DIR)
	$(INSTALL) -c -d $(DSTROOT)/$(LIBEXEC_LIB_DIR)

	set -e; for i in $(FRAMEWORKS); do \
		j=`echo $${i} | sed -e 's/liberty/libiberty/;' -e 's/binutils/\./;'`; \
		fdir=$(OBJROOT)/$(NATIVE_TARGETS)/$${j}/$${i}.framework; \
		pdir=$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework; \
		cp -p $${fdir}/Versions/A/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX) \
			$(SYMROOT)/$${pdir}/Versions/A/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX); \
		cp -p $${fdir}/Versions/A/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX) \
			$(DSTROOT)/$${pdir}/Versions/A/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX); \
		strip $(DSTROOT)/$${pdir}/Versions/A/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX); \
		for k in $(SYMROOT) $(DSTROOT); do \
			ln -s $(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX) \
				$${k}/$${pdir}/Versions/A/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_SUFFIX); \
			ln -s Versions/Current/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX) \
				$${k}/$${pdir}/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX); \
			ln -s Versions/Current/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_SUFFIX) \
				$${k}/$${pdir}/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_SUFFIX); \
			ln -s $(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_SUFFIX) \
				$${k}/$${pdir}/$${i}; \
			ln -s ../../Library/PrivateFrameworks/$${i}.framework/lib$${i}$(FRAMEWORK_SUFFIX) \
				$${k}/$(LIBEXEC_LIB_DIR)/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_SUFFIX); \
			ln -s ../../Library/PrivateFrameworks/$${i}.framework/Versions/A/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX) \
				$${k}/$(LIBEXEC_LIB_DIR)/$(FRAMEWORK_PREFIX)$${i}$(FRAMEWORK_VERSION_SUFFIX); \
		done; \
	done

install-binutils-common:

	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
		\
		docroot=$${dstroot}/$(DOCUMENTATION_DIR)/binutils; \
		\
		$(INSTALL) -c -d $${docroot}/binutils; \
		\
		for i in $(BINUTILS_MANPAGES); do \
			$(INSTALL) -c -m 644 $(SRCROOT)/src/binutils/$${i} $${docroot}/`echo $${i} | sed -e 's/cxxfilt\\.man/cxxfilt.1/'`; \
		done; \
	done;

install-binutils-rhapsody-common: install-binutils-common

	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
		\
		$(INSTALL) -c -d $${dstroot}/$(LIBEXEC_BINUTILS_DIR); \
		\
		docroot=$${dstroot}/$(DOCUMENTATION_DIR)/binutils; \
		\
		(cd $${docroot}/binutils && \
			$(SRCROOT)/texi2html $(SRCROOT)/src/binutils/binutils.texi); \
	done

	set -e; for i in $(BINUTILS_BINARIES); do \
		instname=`echo $${i} | sed -e 's/\\-new//'`; \
		lipo -create $(patsubst %,$(OBJROOT)/%/binutils/$${i},$(NATIVE_TARGETS)) \
			-output $(SYMROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname}; \
		strip -S -o $(DSTROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname} $(SYMROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname}; \
	done

install-binutils-fat: install-binutils-rhapsody-common

install-binutils-rhapsody: install-binutils-rhapsody-common

install-binutils-pdo: install-binutils-common

	set -e;	for dstroot in $(SYMROOT) $(DSTROOT); do \
		\
		docroot=$${dstroot}/$(DOCUMENTATION_DIR)/binutils; \
		\
		for i in binutils; do \
			cp -rp $(SRCROOT)/doc/$${i}.html $${docroot}/$${i}; \
		done; \
	done

	set -e; for i in $(BINUTILS_BINARIES); do \
		instname=`echo $${i} | sed -e 's/\\-new//'`; \
		$(INSTALL) -c $(patsubst %,$(OBJROOT)/%/binutils/$${i},$(NATIVE_TARGET)) \
			$(SYMROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname}; \
		$(INSTALL) -c -s $(patsubst %,$(OBJROOT)/%/binutils/$${i},$(NATIVE_TARGET)) \
			$(DSTROOT)/$(LIBEXEC_BINUTILS_DIR)/$${instname}; \
	done

install-chmod-rhapsody:
	set -e;	if [ `whoami` = 'root' ]; then \
		for dstroot in $(SYMROOT) $(DSTROOT); do \
			chown -R root.wheel $${dstroot}; \
			chmod -R  u=rwX,g=rX,o=rX $${dstroot}; \
			for i in $(FRAMEWORKS); do \
				chmod a+x $${dstroot}/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/$${i}; \
			done; \
			true || chmod a+x $${dstroot}/$(LIBEXEC_GDB_DIR)/*; \
			chmod -R a+x $${dstroot}/$(LIBEXEC_BINUTILS_DIR); \
			true || chmod -R a+x $${dstroot}/$(DEVEXEC_DIR); \
		done; \
	fi

install-chmod-pdo:
	set -e;	if [ `whoami` = 'root' ]; then \
		true; \
	fi

install-source:
	$(INSTALL) -c -d $(DSTROOT)/$(SOURCE_DIR)
	$(TAR) --exclude=CVS -C $(SRCROOT) -cf - . | $(TAR) -C $(DSTROOT)/$(SOURCE_DIR) -xf -

all: build

clean:
	$(RM) -r $(OBJROOT)

check-args:
ifeq "$(CANONICAL_ARCHS)" "i386-apple-rhapsody"
else
ifeq "$(CANONICAL_ARCHS)" "powerpc-apple-rhapsody"
else
ifeq "$(CANONICAL_ARCHS)" "i386-apple-macos10"
else
ifeq "$(CANONICAL_ARCHS)" "powerpc-apple-macos10"
else
ifeq "$(CANONICAL_ARCHS)" "i386-apple-rhapsody powerpc-apple-rhapsody"
else
ifeq "$(CANONICAL_ARCHS)" "i386-apple-macos10 powerpc-apple-macos10"
else
ifeq "$(CANONICAL_ARCHS)" "powerpc-apple-rhapsody i386-apple-rhapsody"
else
ifeq "$(CANONICAL_ARCHS)" "powerpc-apple-macos10 i386-apple-macos10"
else
ifeq "$(CANONICAL_ARCHS)" "hppa1.1-nextpdo-hpux10.20"
else
ifeq "$(CANONICAL_ARCHS)" "hppa2.0n-nextpdo-hpux11.0"
else
ifeq "$(CANONICAL_ARCHS)" "sparc-nextpdo-solaris2"
else
	echo "Unknown architecture string: \"$(CANONICAL_ARCHS)\""
	exit 1
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif
endif

configure: 
ifneq ($(findstring rhapsody,$(CANONICAL_ARCHS))$(findstring macos10,$(CANONICAL_ARCHS)),)
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-rc-configure, $(NATIVE_TARGETS))
endif
else
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-rc-configure-pdo, $(NATIVE_TARGETS))
endif
endif

build-headers:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-headers, $(NATIVE_TARGETS)) 
endif

build-core:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-core, $(NATIVE_TARGETS)) 
endif

build-binutils:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-binutils, $(NATIVE_TARGETS))
endif

build:
	$(SUBMAKE) check-args
	$(SUBMAKE) build-core
	$(SUBMAKE) build-binutils

install-clean:
	$(RM) -r $(SYMROOT) $(DSTROOT)

install-rhapsody:
	$(SUBMAKE) install-clean
ifeq "$(CANONICAL_ARCHS)" "i386-apple-rhapsody powerpc-apple-rhapsody"
	$(SUBMAKE) install-frameworks-rhapsody NATIVE_TARGET=unknown--unknown
	$(SUBMAKE) install-binutils-fat NATIVE_TARGET=unknown--unknown
else
ifeq "$(CANONICAL_ARCHS)" "i386-apple-macos10 powerpc-apple-macos10"
	$(SUBMAKE) install-frameworks-rhapsody NATIVE_TARGET=unknown--unknown
	$(SUBMAKE) install-binutils-fat NATIVE_TARGET=unknown--unknown
else
	$(SUBMAKE) install-frameworks-rhapsody
	$(SUBMAKE) install-binutils-rhapsody
endif
endif
	$(SUBMAKE) install-chmod-rhapsody

install-pdo:
	$(SUBMAKE) install-clean
	$(SUBMAKE) install-frameworks-pdo NATIVE_TARGET=$(NATIVE_TARGETS)
	$(SUBMAKE) install-binutils-pdo NATIVE_TARGET=$(NATIVE_TARGETS)
	$(SUBMAKE) install-chmod-pdo

install-src:
	$(SUBMAKE) check-args
	$(SUBMAKE) build
ifneq ($(findstring rhapsody,$(CANONICAL_ARCHS))$(findstring macos10,$(CANONICAL_ARCHS)),)
	$(SUBMAKE) $(RHAPSODY_FLAGS) install-rhapsody
	$(SUBMAKE) $(RHAPSODY_FLAGS) install-source
	$(SUBMAKE) $(RHAPSODY_FLAGS) install-chmod-rhapsody
else
	$(SUBMAKE) $(PDO_FLAGS) install-pdo
	$(SUBMAKE) $(PDO_FLAGS) install-source
	$(SUBMAKE) $(PDO_FLAGS) install-chmod-pdo
endif

install-nosrc:
	$(SUBMAKE) check-args
	$(SUBMAKE) build
ifneq ($(findstring rhapsody,$(CANONICAL_ARCHS))$(findstring macos10,$(CANONICAL_ARCHS)),)
	$(SUBMAKE) $(RHAPSODY_FLAGS) install-rhapsody
else
	$(SUBMAKE) $(PDO_FLAGS) install-pdo
endif

install:
ifeq ($(INSTALL_SOURCE),no)
	$(SUBMAKE) install-nosrc
else	
#	$(SUBMAKE) install-src
	$(SUBMAKE) install-nosrc
endif

installhdrs:
	$(SUBMAKE) check-args
	$(SUBMAKE) configure 
	$(SUBMAKE) build-headers
	$(SUBMAKE) install-clean
ifneq ($(findstring rhapsody,$(CANONICAL_ARCHS))$(findstring macos10,$(CANONICAL_ARCHS)),)
	$(SUBMAKE) $(RHAPSODY_FLAGS) CURRENT_ROOT=$(SYMROOT) install-frameworks-headers
	$(SUBMAKE) $(RHAPSODY_FLAGS) CURRENT_ROOT=$(DSTROOT) install-frameworks-headers
else
	$(SUBMAKE) $(PDO_FLAGS) CURRENT_ROOT=$(SYMROOT) install-frameworks-headers
	$(SUBMAKE) $(PDO_FLAGS) CURRENT_ROOT=$(DSTROOT) install-frameworks-headers
endif

installsrc:
	$(SUBMAKE) check
	$(TAR) --dereference --exclude=CVS -cf - . | $(TAR) -C $(SRCROOT) -xf -

check:
	[ -z `find . -name \*~ -o -name .\#\*` ] || \
		(echo 'Emacs or CVS backup files present; not copying.' && exit 1)
