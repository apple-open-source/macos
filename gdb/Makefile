.PHONY: all clean configure build install installsrc installhdrs \
	build-core build-gdb \
	install-gdb-pdo install-gdb-fat \
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

GDB_VERSION = 5.1-20020408
APPLE_VERSION = 231

GDB_VERSION_STRING = $(GDB_VERSION) (Apple version gdb-$(APPLE_VERSION))

GDB_BINARY = gdb
FRAMEWORKS = gdb readline

ifndef BINUTILS_BUILD_ROOT
BINUTILS_BUILD_ROOT = $(NEXT_ROOT)
endif

ifneq ($(findstring rhapsody,$(CANONICAL_ARCHS))$(findstring macos10,$(CANONICAL_ARCHS)),)
BINUTILS_FRAMEWORKS = $(BINUTILS_BUILD_ROOT)/System/Library/PrivateFrameworks
else
BINUTILS_FRAMEWORKS = $(BINUTILS_BUILD_ROOT)/Library/PrivateFrameworks
endif

BFD_FRAMEWORK = $(BINUTILS_FRAMEWORKS)/bfd.framework
BFD_HEADERS = $(BFD_FRAMEWORK)/Headers

LIBERTY_FRAMEWORK = $(BINUTILS_FRAMEWORKS)/liberty.framework
LIBERTY_HEADERS = $(LIBERTY_FRAMEWORK)/Headers

MMALLOC_FRAMEWORK = $(BINUTILS_FRAMEWORKS)/mmalloc.framework
MMALLOC_HEADERS = $(MMALLOC_FRAMEWORK)/Headers

OPCODES_FRAMEWORK = $(BINUTILS_FRAMEWORKS)/opcodes.framework
OPCODES_HEADERS = $(OPCODES_FRAMEWORK)/Headers

BINUTILS_FRAMEWORK = $(BINUTILS_FRAMEWORKS)/binutils.framework
BINUTILS_HEADERS = $(BINUTILS_FRAMEWORK)/Headers

READLINE_FRAMEWORK = $(BINUTILS_FRAMEWORKS)/readline.framework
READLINE_HEADERS = $(READLINE_FRAMEWORK)/Headers

EFENCE_FRAMEWORK = $(BINUTILS_FRAMEWORKS)/electric-fence.framework
EFENCE_HEADERS = $(EFENCE_FRAMEWORK)/Headers

TAR = gnutar

CC = cc
CC_FOR_BUILD = cc
CDEBUGFLAGS = -g
CFLAGS = $(CDEBUGFLAGS) $(RC_CFLAGS)
HOST_ARCHITECTURE = UNKNOWN

RC_CFLAGS_NOARCH = $(shell echo $(RC_CFLAGS) | sed -e 's/-arch [a-z0-9]*//g')

SYSTEM_FRAMEWORK = -framework System
FRAMEWORK_PREFIX =
FRAMEWORK_SUFFIX =
FRAMEWORK_VERSION = A
FRAMEWORK_VERSION_SUFFIX =

CONFIG_DIR=UNKNOWN
CONF_DIR=UNKNOWN
DEVEXEC_DIR=UNKNOWN
DOCUMENTATION_DIR=UNKNOWN
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
CONFIG_ENABLE_GDBMI=--enable-gdbmi=yes
CONFIG_ENABLE_BUILD_WARNINGS=--enable-build-warnings
CONFIG_ENABLE_TUI=--disable-tui
CONFIG_ALL_BFD_TARGETS=
CONFIG_64_BIT_BFD=--enable-64-bit-bfd
CONFIG_WITH_MMAP=--with-mmap
CONFIG_WITH_MMALLOC=--with-mmalloc
CONFIG_MAINTAINER_MODE=--enable-maintainer-mode
CONFIG_OTHER_OPTIONS=

ifeq ($(RC_RELEASE),Darwin)
MAKE_CFM=WITHOUT_CFM=1
else
MAKE_CFM=
endif
 
MAKE_CTHREADS=

ifneq ($(findstring rhapsody,$(CANONICAL_ARCHS))$(findstring macos10,$(CANONICAL_ARCHS)),)
CC = cc -arch $(HOST_ARCHITECTURE) -no-cpp-precomp
CC_FOR_BUILD = NEXT_ROOT= cc -no-cpp-precomp
CDEBUGFLAGS = -g -Os
CFLAGS = $(CDEBUGFLAGS) -Wall -Wimplicit -Wno-long-double $(RC_CFLAGS_NOARCH)
HOST_ARCHITECTURE = $(shell echo $* | sed -e 's/--.*//' -e 's/powerpc/ppc/' -e 's/-apple-rhapsody//' -e 's/-apple-macos.*//')
endif

ifneq ($(findstring rhapsody,$(CANONICAL_ARCHS)),)
MAKE_CTHREADS=USE_CTHREADS=1
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

RHAPSODY_FLAGS = \
	CONFIG_DIR=private/etc \
	CONF_DIR=usr/share/gdb \
	DEVEXEC_DIR=usr/bin \
	DOCUMENTATION_DIR=Developer/Documentation/DeveloperTools \
	LIBEXEC_BINUTILS_DIR=usr/libexec/binutils \
	LIBEXEC_GDB_DIR=usr/libexec/gdb \
	MAN_DIR=usr/share/man \
	PRIVATE_FRAMEWORKS_DIR=System/Library/PrivateFrameworks \
	SOURCE_DIR=System/Developer/Source/Commands/gdb

PDO_FLAGS = \
	CONFIG_DIR=Developer/Libraries/gdb \
	CONF_DIR=Developer/Libraries/gdb \
	DEVEXEC_DIR=Developer/Executables \
	DOCUMENTATION_DIR=Documentation/Developer/DeveloperTools \
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
	$(CONFIG_ENABLE_GDBTK) \
	$(CONFIG_ENABLE_GDBMI) \
	$(CONFIG_ALL_BFD_TARGETS) \
	$(CONFIG_64_BIT_BFD) \
	$(CONFIG_WITH_MMAP) \
	$(CONFIG_WITH_MMALLOC) \
	$(CONFIG_OTHER_OPTIONS)

MAKE_OPTIONS = \
	$(MAKE_CFM) \
	$(MAKE_CTHREADS)

EFLAGS = \
	CFLAGS='$(CFLAGS)' \
	CC='$(CC)' \
	CC_FOR_BUILD='$(CC_FOR_BUILD)' \
	HOST_ARCHITECTURE='$(HOST_ARCHITECTURE)' \
	NEXT_ROOT='$(NEXT_ROOT)' \
	BINUTILS_FRAMEWORKS='$(BINUTILS_FRAMEWORKS)' \
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
	MMALLOC_DEP='$(MMALLOC_FRAMEWORK)/mmalloc' \
	MMALLOC='-F$(BINUTILS_FRAMEWORKS) -framework mmalloc' \
	MMALLOC_CFLAGS='-I$(MMALLOC_HEADERS)' \
	OPCODES_DEP='$(OPCODES_FRAMEWORK)/opcodes' \
	OPCODES='-F$(BINUTILS_FRAMEWORKS) -framework opcodes' \
	OPCODES_CFLAGS='-I$(OPCODES_HEADERS)' \
	BFD_DIR='$(BFD_HEADERS)' \
	BFD_SRC='$(BFD_HEADERS)' \
	BFD_DEP='$(BFD_FRAMEWORK)/bfd' \
	BFD='-F$(BINUTILS_FRAMEWORKS) -framework bfd' \
	BFD_CFLAGS='-I$(BFD_HEADERS)' \
	LIBIBERTY_DEP='$(LIBERTY_FRAMEWORK)/liberty' \
	LIBIBERTY='-F$(BINUTILS_FRAMEWORKS) -framework liberty' \
	LIBIBERTY_CFLAGS='-I$(LIBERTY_HEADERS)' \
	INCLUDE_DIR='$(BINUTILS_HEADERS)' \
	INCLUDE_CFLAGS='-I$(BINUTILS_HEADERS)' \
	EFENCE_DEP='$(EFENCE_FRAMEWORK)/electric-fence' \
	EFENCE='-F$(BINUTILS_FRAMEWORKS) -framework electric-fence' \
	EFENCE_CFLAGS='-I$(EFENCE_HEADERS)' 

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

$(OBJROOT)/%/stamp-rc-configure-cross:
	$(RM) -r $(OBJROOT)/$*
	$(INSTALL) -c -d $(OBJROOT)/$*/gdb
	(cd $(OBJROOT)/$*/gdb && \
		$(CONFIGURE_ENV) $(SRCTOP)/src/gdb/configure \
			--host=$(shell echo $* | sed -e 's/--.*//') \
			--target=$(shell echo $* | sed -e 's/.*--//') \
			$(CONFIGURE_OPTIONS) \
			)
	ln -sf ../$(shell echo $* | sed -e 's/\(.*\)--.*/\1--\1/')/readline $(OBJROOT)/$*/
	touch $@

$(OBJROOT)/%/stamp-build-gdb-pdo:
	$(SUBMAKE) -C $(OBJROOT)/$*/readline $(MFLAGS) all
	$(SUBMAKE) -C $(OBJROOT)/$*/gdb $(MFLAGS) $(FSFLAGS) VERSION='$(GDB_VERSION_STRING)' gdb
	#touch $@

$(OBJROOT)/%/stamp-build-headers:
	#$(SUBMAKE) -C $(OBJROOT)/$*/texinfo $(MFLAGS) CC='$(CC_FOR_BUILD)' all
	$(SUBMAKE) -C $(OBJROOT)/$*/gdb/doc $(MFLAGS) VERSION='$(GDB_VERSION_STRING)'
	$(SUBMAKE) -C $(OBJROOT)/$* $(MFLAGS) stamp-framework-headers-gdb
	#touch $@

$(OBJROOT)/%/stamp-build-core:
	$(SUBMAKE) -C $(OBJROOT)/$*/readline $(MFLAGS) all stamp-framework
	$(SUBMAKE) -C $(OBJROOT)/$*/intl $(MFLAGS)
	#touch $@

$(OBJROOT)/%/stamp-build-gdb:
	$(SUBMAKE) -C $(OBJROOT)/$*/gdb $(MFLAGS) $(FSFLAGS) VERSION='$(GDB_VERSION_STRING)' gdb
	if echo $* | egrep '^[^-]*-apple-macos10' > /dev/null; then \
		echo "stripping __objcInit"; \
		echo "__objcInit" > /tmp/macosx-syms-to-remove; \
		strip -R /tmp/macosx-syms-to-remove -X $(OBJROOT)/$*/gdb/gdb || true; \
		rm -f /tmp/macosx-syms-to-remove; \
	fi; \

$(OBJROOT)/%/stamp-build-gdb-framework:
	$(SUBMAKE) -C $(OBJROOT)/$* $(FFLAGS) stamp-framework-gdb

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
		j=`echo $${i} | sed -e 's/liberty/libiberty/;' -e 's/binutils/\./;' -e 's/gdb/\./;'`; \
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
		j=`echo $${i} | sed -e 's/liberty/libiberty/;' -e 's/binutils/\./;' -e 's/gdb/\./;'`; \
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

install-gdb-common:

	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
		\
		$(INSTALL) -c -d $${dstroot}/$(DEVEXEC_DIR); \
		$(INSTALL) -c -d $${dstroot}/$(CONFIG_DIR); \
		$(INSTALL) -c -d $${dstroot}/$(CONF_DIR); \
		$(INSTALL) -c -d $${dstroot}/$(MAN_DIR); \
		\
		docroot=$${dstroot}/$(DOCUMENTATION_DIR)/gdb; \
		\
		$(INSTALL) -c -d $${docroot}; \
		\
		$(INSTALL) -c -m 644 $(SRCROOT)/src/gdb/gdb.1 $${docroot}/gdb.1; \
		\
		$(INSTALL) -c -m 644 $(SRCROOT)/doc/refcard.pdf $${docroot}/refcard.pdf; \
		\
	done;

install-gdb-pdo: install-gdb-common

	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
		\
		docroot=$${dstroot}/$(DOCUMENTATION_DIR)/gdb; \
		\
		for i in stabs gdb gdbint; do \
			cp -rp $(SRCROOT)/doc/$${i}.html $${docroot}/$${i}; \
		done; \
		\
		$(INSTALL) -c -d $${dstroot}/$(MAN_DIR)/man1; \
		$(INSTALL) -c -m 644 $(SRCROOT)/src/gdb/gdb.1 $${dstroot}/$(MAN_DIR)/man1/gdb.1; \
		\
		$(INSTALL) -c -d $${dstroot}/$(CONFIG_DIR); \
		$(INSTALL) -c -m 644 $(SRCROOT)/gdb.conf $${dstroot}/$(CONFIG_DIR)/gdb.conf; \
		\
		$(INSTALL) -c -d $${dstroot}/$(CONF_DIR); \
		for j in $(SRCROOT)/conf/*.gdb; do \
			$(INSTALL) -c -m 644 $$j $${dstroot}/$(CONF_DIR)/; \
		done; \
	done;

	$(INSTALL) -c $(OBJROOT)/$(NATIVE_TARGET)/gdb/gdb $(SYMROOT)/$(DEVEXEC_DIR)/gdb
	$(INSTALL) -c -s $(OBJROOT)/$(NATIVE_TARGET)/gdb/gdb $(DSTROOT)/$(DEVEXEC_DIR)/gdb

install-gdb-rhapsody-common: install-gdb-common

	set -e; for dstroot in $(SYMROOT) $(DSTROOT); do \
		\
		$(INSTALL) -c -d $${dstroot}/$(LIBEXEC_GDB_DIR); \
		\
		docroot=$${dstroot}/$(DOCUMENTATION_DIR)/gdb; \
		\
		for i in gdb gdbint stabs; do \
			$(INSTALL) -c -d $${docroot}/$${i}; \
			(cd $${docroot}/$${i} && \
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
		\
		$(INSTALL) -c -d $${dstroot}/$(CONFIG_DIR); \
		$(INSTALL) -c -m 644 $(SRCROOT)/gdb.conf $${dstroot}/$(CONFIG_DIR)/gdb.conf; \
		\
		$(INSTALL) -c -d $${dstroot}/$(CONF_DIR); \
		for j in $(SRCROOT)/conf/*.gdb; do \
			$(INSTALL) -c -m 644 $$j $${dstroot}/$(CONF_DIR)/; \
		done; \
		\
		sed -e 's/version=.*/version=$(GDB_VERSION)-$(APPLE_VERSION)/' \
			< $(SRCROOT)/gdb.sh > $${dstroot}/usr/bin/gdb; \
		chmod 755 $${dstroot}/usr/bin/gdb; \
		\
		cp -p $(SRCROOT)/cache-symfiles.sh $${dstroot}/usr/libexec/gdb/cache-symfiles; \
		chmod 755 $${dstroot}/usr/libexec/gdb/cache-symfiles; \
		\
	done;

install-gdb-rhapsody: install-gdb-rhapsody-common

	set -e; for target in $(CANONICAL_ARCHS); do \
		lipo -create $(OBJROOT)/$${target}--$${target}/gdb/gdb \
			-output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
	 	strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} \
			$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
	done

install-gdb-fat: install-gdb-rhapsody-common

	lipo -create $(patsubst %,$(OBJROOT)/%/gdb/gdb,$(PPC_TARGET)--$(PPC_TARGET) $(I386_TARGET)--$(PPC_TARGET)) \
		-output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$(PPC_TARGET)
	lipo -create $(patsubst %,$(OBJROOT)/%/gdb/gdb,$(PPC_TARGET)--$(I386_TARGET) $(I386_TARGET)--$(I386_TARGET)) \
		-output $(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$(I386_TARGET)

	set -e; for target in $(CANONICAL_ARCHS); do \
	 	strip -S -o $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} \
			$(SYMROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target}; \
		if echo $${target} | egrep '^[^-]*-apple-macos10' > /dev/null; then \
			echo "stripping __objcInit"; \
			echo "__objcInit" > /tmp/macosx-syms-to-remove; \
			strip -R /tmp/macosx-syms-to-remove -X $(DSTROOT)/$(LIBEXEC_GDB_DIR)/gdb-$${target} || true; \
			rm -f /tmp/macosx-syms-to-remove; \
		fi; \
	done

install-chmod-rhapsody:
	set -e;	if [ `whoami` = 'root' ]; then \
		for dstroot in $(SYMROOT) $(DSTROOT); do \
			chown -R root.wheel $${dstroot}; \
			chmod -R  u=rwX,g=rX,o=rX $${dstroot}; \
			for i in $(FRAMEWORKS); do \
				chmod a+x $${dstroot}/$(PRIVATE_FRAMEWORKS_DIR)/$${i}.framework/Versions/A/$${i}; \
			done; \
			chmod a+x $${dstroot}/$(LIBEXEC_GDB_DIR)/*; \
			chmod a+x $${dstroot}/$(DEVEXEC_DIR)/*; \
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
ifneq ($(CROSS_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-rc-configure-cross, $(CROSS_TARGETS))
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

build-gdb:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb, $(NATIVE_TARGETS))
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb-framework, $(NATIVE_TARGETS))
endif
ifneq ($(CROSS_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb, $(CROSS_TARGETS))
endif

build-gdb-pdo:
	$(SUBMAKE) configure
ifneq ($(NATIVE_TARGETS),)
	$(SUBMAKE) $(patsubst %,$(OBJROOT)/%/stamp-build-gdb-pdo, $(NATIVE_TARGETS)) 
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
ifneq ($(findstring rhapsody,$(CANONICAL_ARCHS))$(findstring macos10,$(CANONICAL_ARCHS)),)
	$(SUBMAKE) build-core
	$(SUBMAKE) build-gdb 
	$(SUBMAKE) build-gdb-docs 
else
	$(SUBMAKE) build-gdb-pdo
endif

install-clean:
	$(RM) -r $(SYMROOT) $(DSTROOT)

install-rhapsody:
	$(SUBMAKE) install-clean
ifeq "$(CANONICAL_ARCHS)" "i386-apple-rhapsody powerpc-apple-rhapsody"
	$(SUBMAKE) install-frameworks-rhapsody NATIVE_TARGET=unknown--unknown
	$(SUBMAKE) install-gdb-fat NATIVE_TARGET=unknown--unknown PPC_TARGET=powerpc-apple-rhapsody I386_TARGET=i386-apple-rhapsody
else
ifeq "$(CANONICAL_ARCHS)" "i386-apple-macos10 powerpc-apple-macos10"
	$(SUBMAKE) install-frameworks-rhapsody NATIVE_TARGET=unknown--unknown
	$(SUBMAKE) install-gdb-fat NATIVE_TARGET=unknown--unknown PPC_TARGET=powerpc-apple-macos10 I386_TARGET=i386-apple-macos10
	$(SUBMAKE) install-macsbug
else
ifeq "$(CANONICAL_ARCHS)" "powerpc-apple-macos10"
	$(SUBMAKE) install-frameworks-rhapsody
	$(SUBMAKE) install-gdb-rhapsody
	$(SUBMAKE) install-macsbug
else
	$(SUBMAKE) install-frameworks-rhapsody
	$(SUBMAKE) install-gdb-rhapsody
endif
endif
endif
	$(SUBMAKE) install-chmod-rhapsody

install-macsbug:
	$(SUBMAKE) -C $(SRCROOT)/macsbug GDB_BUILD_ROOT=$(DSTROOT) SRCROOT=$(SRCROOT)/macsbug OBJROOT=$(OBJROOT)/powerpc-apple-macos10--powerpc-apple-macos10/macsbug SYMROOT=$(SYMROOT) DSTROOT=$(DSTROOT) install
 
install-pdo:
	$(SUBMAKE) install-clean
	$(SUBMAKE) install-frameworks-pdo NATIVE_TARGET=$(NATIVE_TARGETS)
	$(SUBMAKE) install-gdb-pdo NATIVE_TARGET=$(NATIVE_TARGETS)
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
ifeq ($(INSTALL_SOURCE),yes)
	$(SUBMAKE) install-src
else	
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
