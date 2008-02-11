#
# Copyright (c) 2001-2006 Apple Computer, Inc.
#
# Wrapper Makefile for ChatServer
#
# ChatServer consists of two main packages: jabberd2 and libidn.  The libidn
# package in built and installed into a temporary location prior to configuring
# jabberd2.  There is no install target for libidn -- only jabberd2 installs 
# any files in the system.
#

# These includes provide the proper paths to system utilities

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

#
# Special variables for make targets
#
# The temporary libidn folder structure is created as follows:
#    $(OBJROOT)/
#        libidn.build/
#            src/
#                ppc/
#                i386/
#                ...
#            lib/
#                ppc/
#                i386/
#                ...
#            include/
# NOTE: Since the value of $(OBJROOT) varies depending on whether ~rc/bin/buildit
#       is invoked or make from the ChatServer2 directory, the location of the 
#       libidn temp directories will vary accordinly.  As additional architectures
#       are added to the Makefile, additional sub-folders will be created under 
#       src/ and lib/ as needed.
#
PROJECT_NAME	= ChatServer

# STAGING DIR
#
# As of the current release, jabberd2 and certain other package sources are build
# into a "staging" directory, which contains all the build products, but is not
# organized in the final intallation structure.  The contents of STAGING_DIR
# will be copied in the install phase (see below) to create the structure that
# will exists when Mac OS X Server is intalled.
STAGING_DIR 	:= staging

# common root directories with absolute paths
MYSQL_LIB_DIR	= /usr/lib/mysql
SQLITE_LIB_DIR	= /usr/lib
SQLITE_INCLUDE_DIR	= /usr/include
BUILD_DIR	= /usr

# common directories with relative paths (may be contained inside build directories)
ETC_DIR	= private/etc
VAR_DIR=private/var
USRBIN_DIR=usr/bin
USRLIB_DIR=usr/lib
USRLOCAL_DIR=usr/local
USRSHARE_DIR=usr/share
LIBEXEC_DIR=usr/libexec
SYSCONFIG_DIR	= usr/local/etc
MAN_DIR	= usr/share/man
INFO_DIR	= usr/share/info
MIGRATION_EXTRAS_DIR	= System/Library/ServerSetup/MigrationExtras

# tool path defines
INSTALL		= /usr/bin/install
DITTO		= /usr/bin/ditto
TAR		= /usr/bin/tar
GZIP		= /usr/bin/gzip
MKDIR		= /bin/mkdir
PATCH		=/usr/bin/patch
GCC		=/usr/bin/gcc
LS		=/bin/ls
APPLELIBTOOL		=/usr/bin/libtool

#-------------------------------
# Define the default build rule
#-------------------------------
#
default: build


#-------------------------------
# LIBIDN MODULE
#-------------------------------
#
# NOTE: There is no install target for libidn.  It's make output is copied 
#       to $(OBJROOT)/$(LIBIDN_BUILD_DIR) so that the jabberd2 target can 
#       statically link against the library during the build, but the 
#       intermediate build results are discarded after the build completes.
#
# DIRECTORIES:
#       Sources:
#          tar output: $(SRCROOT)/$(LIBIDN_NAME)/
#          make sources (ppc): $(OBJROOT)/$(LIBIDN_SRC_DIR)/ppc/
#          make sources (i386): $(OBJROOT)/$(LIBIDN_SRC_DIR)/i386/
#       Build Products:
#          libraries (ppc): $(OBJROOT)/$(LIBIDN_LIB_DIR)/ppc/
#          libraries (i386): $(OBJROOT)/$(LIBIDN_LIB_DIR)/i386/
#          libraries (universal): $(OBJROOT)/$(LIBIDN_LIB_DIR)/
#          includes (universal): $(OBJROOT)/$(LIBIDN_INCLUDE_DIR)/
#
LIBIDN_VERSION	= libidn-0.6.0
LIBIDN_NAME	= libidn
LIBIDN_A	= libidn.a

LIBIDN_BUILD_DIR	:= libidn.build
LIBIDN_SRC_DIR	:= $(LIBIDN_BUILD_DIR)/src
LIBIDN_LIB_DIR	:= $(LIBIDN_BUILD_DIR)/lib
LIBIDN_INCLUDE_DIR	:= $(LIBIDN_BUILD_DIR)/include

LIBIDN_ARCHS	:= ppc i386

# libidn build rules
#

.PHONY: libidn/untar libidn/build libidn/build-lib libidn/clean libidn/cleanobj libidn/clean-all

$(LIBIDN_NAME): $(OBJROOT)
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: UNPACKING"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(LIBIDN_SRC_DIR); $(MKDIR) -p -m 755 $(OBJROOT)/$(LIBIDN_SRC_DIR)
	for i in $(LIBIDN_ARCHS) ; do \
		$(SILENT) $(CD) $(OBJROOT)/$(LIBIDN_SRC_DIR); $(TAR) -xzf $(SRCROOT)/$(LIBIDN_VERSION).tgz; $(MV) $(LIBIDN_VERSION) $$i; \
	done

libidn/untar: $(LIBIDN_NAME)

$(LIBIDN_NAME)/config.status: libidn/untar
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: CONFIGURING"
	@echo "#"
	for i in $(LIBIDN_ARCHS) ; do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...configuring; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(LIBIDN_SRC_DIR)/$$i; ./configure --prefix=$(BUILD_DIR) --target=$$i-apple-darwin; \
	done

$(LIBIDN_NAME)/lib/.libs/libidn.dylib:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: BUILDING"
	@echo "#"
	for i in $(LIBIDN_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...building; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(LIBIDN_SRC_DIR)/$$i; make CFLAGS="-arch $$i"; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: INSTALLING"
	@echo "#"
	for i in $(LIBIDN_ARCHS) ; do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...installing - caching make results: libs; arch=$$i"; \
		$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(LIBIDN_INCLUDE_DIR)/$$i; \
		$(SILENT) $(DITTO) $(OBJROOT)/$(LIBIDN_SRC_DIR)/$$i/lib/.libs/ $(OBJROOT)/$(LIBIDN_LIB_DIR)/$$i/; \
	done
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...installing - caching make results: includes"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(LIBIDN_INCLUDE_DIR)
	$(SILENT) $(DITTO) $(OBJROOT)/$(LIBIDN_SRC_DIR)/ppc/lib/*.h $(OBJROOT)/$(LIBIDN_INCLUDE_DIR)/
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...installing - creating univeral library"
	$(SILENT) $(LIPO) -create \
		-arch ppc $(OBJROOT)/$(LIBIDN_LIB_DIR)/ppc/$(LIBIDN_A)  \
		-arch i386 $(OBJROOT)/$(LIBIDN_LIB_DIR)/i386/$(LIBIDN_A) \
		-output $(OBJROOT)/$(LIBIDN_LIB_DIR)/$(LIBIDN_A)
	$(SILENT) $(LIPO) -info $(OBJROOT)/$(LIBIDN_LIB_DIR)/$(LIBIDN_A)

libidn/build: $(LIBIDN_NAME)/config.status $(LIBIDN_NAME)/lib/.libs/libidn.dylib

libidn/clean:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: cleaning sources"
	@echo "#"
	$(SILENT) $(RM) -rf $(LIBIDN_VERSION) $(LIBIDN_NAME) $(OBJROOT)/$(LIBIDN_BUILD_DIR)
	$(SILENT) $(TAR) -xzf $(LIBIDN_VERSION).tgz; $(MV) $(LIBIDN_VERSION) $(LIBIDN_NAME)

libidn/cleanobj:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: cleaning objects"
	@echo "#"
	for i in $(LIBIDN_ARCHS) ; do \
		$(SILENT) $(CD) $(OBJROOT)/$(LIBIDN_SRC_DIR)/$$i; make clean; \
	done

libidn/clean-all:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: cleaning ALL"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(LIBIDN_BUILD_DIR)


#-------------------------------
# PKG-CONFIG MODULE
#-------------------------------
#
# NOTE: There is no install target for pkg-config.  It's make output is copied 
#       to $(OBJROOT)/$(PKG_CONFIG_BUILD_DIR) so that the mu-conference target can 
#       use the binary during the build, but the intermediate build results are 
#       discarded after the build completes.
#
# DIRECTORIES:
#       Sources:
#          tar output: $(SRCROOT)/$(PKG_CONFIG_NAME)/
#          make sources (ppc): $(OBJROOT)/$(PKG_CONFIG_SRC_DIR)/ppc/
#          make sources (i386): $(OBJROOT)/$(PKG_CONFIG_SRC_DIR)/i386/
#       Build Products:
#          tool (ppc): $(OBJROOT)/$(PKG_CONFIG_BIN_DIR)/ppc/
#          tool (i386): $(OBJROOT)/$(PKG_CONFIG_BIN_DIR)/i386/
#          tool (universal): $(OBJROOT)/$(PKG_CONFIG_BIN_DIR)/
#

PKG_CONFIG_VERSION	= pkg-config-0.21
PKG_CONFIG_NAME	= pkg-config

PKG_CONFIG_BUILD_DIR	:= pkg-config.build
PKG_CONFIG_SRC_DIR	:= $(PKG_CONFIG_BUILD_DIR)/src
PKG_CONFIG_BIN_DIR	:= $(PKG_CONFIG_BUILD_DIR)/bin

PKG_CONFIG_ARCHS	:= ppc i386

# pkg-config build rules
#

.PHONY: pkg-config/untar pkg-config/build pkg-config/clean pkg-config/cleanobj pkg-config/clean-all

$(PKG_CONFIG_NAME): $(OBJROOT)
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: UNPACKING"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(PKG_CONFIG_SRC_DIR); $(MKDIR) -p -m 755 $(OBJROOT)/$(PKG_CONFIG_SRC_DIR)
	for i in $(PKG_CONFIG_ARCHS) ; do \
		$(SILENT) $(CD) $(OBJROOT)/$(PKG_CONFIG_SRC_DIR); $(TAR) -xzf $(SRCROOT)/$(PKG_CONFIG_VERSION).tgz; $(MV) $(PKG_CONFIG_VERSION) $$i; \
	done

pkg-config/untar: $(PKG_CONFIG_NAME)

$(PKG_CONFIG_NAME)/config.status: pkg-config/untar
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: CONFIGURING"
	@echo "#"
	for i in $(PKG_CONFIG_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: ...configuring; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(PKG_CONFIG_SRC_DIR)/$$i; ./configure --prefix=$(BUILD_DIR) --target=$$i-apple-darwin; \
	done

$(PKG_CONFIG_NAME)/pkg-config: $(PKG_CONFIG_NAME)/config.status
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: BUILDING"
	@echo "#"
	for i in $(PKG_CONFIG_ARCHS); do \
		$(SILENT) $(ECHO)  "#"; \
		$(SILENT) $(ECHO)  "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: ...building; arch=$$i"; \
		$(SILENT) $(ECHO)  "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(PKG_CONFIG_SRC_DIR)/$$i; make CFLAGS="-arch $$i"; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: INSTALLING"
	@echo "#"
	for i in $(PKG_CONFIG_ARCHS); do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: ...installing - caching make results; arch=$$i"; \
		$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(PKG_CONFIG_BIN_DIR)/$$i; \
		$(SILENT) $(DITTO) $(OBJROOT)/$(PKG_CONFIG_SRC_DIR)/$$i/pkg-config $(OBJROOT)/$(PKG_CONFIG_BIN_DIR)/$$i/; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: ...installing - creating univeral library"
	@echo "#"
	$(SILENT) $(LIPO) -create \
		-arch ppc $(OBJROOT)/$(PKG_CONFIG_BIN_DIR)/ppc/pkg-config  \
		-arch i386 $(OBJROOT)/$(PKG_CONFIG_BIN_DIR)/i386/pkg-config \
		-output $(OBJROOT)/$(PKG_CONFIG_BIN_DIR)/pkg-config
	$(SILENT) $(LIPO) -info $(OBJROOT)/$(PKG_CONFIG_BIN_DIR)/pkg-config

pkg-config/build: $(PKG_CONFIG_NAME)/config.status $(PKG_CONFIG_NAME)/pkg-config

pkg-config/clean:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: cleaning sources"
	@echo "#"
	$(SILENT) $(RM) -rf $(PKG_CONFIG_VERSION) $(PKG_CONFIG_NAME) $(OBJROOT)/$(PKG_CONFIG_BUILD_DIR)
	$(SILENT) $(TAR) -xzf $(PKG_CONFIG_VERSION).tgz; $(MV) $(PKG_CONFIG_VERSION) $(PKG_CONFIG_NAME)

pkg-config/cleanobj:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: cleaning objects"
	@echo "#"
	for i in $(PKG_CONFIG_ARCHS) ; do \
		$(SILENT) $(CD) $(OBJROOT)/$(PKG_CONFIG_SRC_DIR)/$$i; make clean; \
	done

pkg-config/clean-all:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [pkg-config]: cleaning ALL"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(PKG_CONFIG_BUILD_DIR)


#-------------------------------
# GETTEXT MODULE
#-------------------------------
#
# NOTE: There is no install target for gettext.  It's make output is copied 
#       to $(OBJROOT)/$(GETTEXT_BUILD_DIR) so that the mu-conference target can 
#       use the binary during the build, but the intermediate build results are 
#       discarded after the build completes.
#
# DIRECTORIES:
#       Sources:
#          tar output: $(SRCROOT)/$(GETTEXT_NAME)/
#          make sources (ppc): $(OBJROOT)/$(GETTEXT_SRC_DIR)/ppc/
#          make sources (i386): $(OBJROOT)/$(GETTEXT_SRC_DIR)/i386/
#       Build Products:
#          /usr/bin/* (ppc): $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/ppc/$(USRBIN_DIR)
#          /usr/bin/* (i386): $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/i386/$(USRBIN_DIR)
#          /usr/bin/* (universal): $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRBIN_DIR)
#          /usr/lib/* (ppc): $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/ppc/$(USRLIB_DIR)
#          /usr/lib/* (i386): $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/i386/$(USRLIB_DIR)
#          /usr/lib/* (universal): $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRLIB_DIR)
#

GETTEXT_VERSION	= gettext-0.15
GETTEXT_NAME	= gettext

GETTEXT_BUILD_DIR	:= gettext.build
GETTEXT_SRC_DIR	:= $(GETTEXT_BUILD_DIR)/src
GETTEXT_INSTALL_DIR	:= $(GETTEXT_BUILD_DIR)/install

# GETTEXT_BIN_FILES_TO_LIPO  List of all thin binaries that need to be lipo'd 
#                            into universal binaries for the gettext component.
GETTEXT_BIN_FILES_TO_LIPO	= \
envsubst \
gettext \
msgattrib \
msgcat \
msgcmp \
msgcomm \
msgconv \
msgen \
msgexec \
msgfilter \
msgfmt \
msggrep \
msginit \
msgmerge \
msgunfmt \
msguniq \
ngettext \
recode-sr-latin \
xgettext

# GETTEXT_LIB_FILES_TO_LIPO  List of all thin binaries that need to be lipo'd 
#                            into universal binaries for the gettext component.
GETTEXT_LIB_FILES_TO_LIPO	= \
libasprintf.a \
libgettextpo.a \
libintl.a \
$(GETTEXT_NAME)/hostname \
$(GETTEXT_NAME)/urlget

GETTEXT_ARCHS	:= ppc i386

# gettext build rules
#
.PHONY: gettext/untar gettext/build gettext/clean gettext/cleanobj gettext/clean-all

$(GETTEXT_NAME): $(OBJROOT)
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: UNPACKING"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(GETTEXT_SRC_DIR); $(MKDIR) -p -m 755 $(OBJROOT)/$(GETTEXT_SRC_DIR)
	for i in $(GETTEXT_ARCHS) ; do \
		$(SILENT) $(CD) $(OBJROOT)/$(GETTEXT_SRC_DIR); $(TAR) -xzf $(SRCROOT)/$(GETTEXT_VERSION).tgz; $(MV) $(GETTEXT_VERSION) $$i; \
	done

gettext/untar: $(GETTEXT_NAME)

$(GETTEXT_NAME)/config.status: gettext/untar
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: CONFIGURING"
	@echo "#"
	for i in $(GETTEXT_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: ...configuring; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(GETTEXT_SRC_DIR)/$$i; ./configure --prefix=$(BUILD_DIR) --target=$$i-apple-darwin --disable-shared; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: ...configuring - patching gettext/gettext-tools/examples/Makefile; arch=$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(GETTEXT_SRC_DIR)/$$i/gettext-tools/examples; $(PATCH) -u Makefile $(SRCROOT)/mfpatch/gettext/gettext-tools/examples/Makefile.patch; \
	done

$(GETTEXT_NAME)/gettext:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: BUILDING"
	@echo "#"
	for i in $(GETTEXT_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: ...building; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(GETTEXT_SRC_DIR)/$$i; make CPPFLAGS="-arch $$i" CFLAGS="-arch $$i" LDFLAGS="-arch $$i"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: ...building - caching make results; arch=$$i"; \
		$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$$i; \
		$(SILENT) $(CD) $(OBJROOT)/$(GETTEXT_SRC_DIR)/$$i; make install DESTDIR=$(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$$i; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: ...building - creating universal binaries"
	@echo "#"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRBIN_DIR)
	for i in $(GETTEXT_BIN_FILES_TO_LIPO); do \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRBIN_DIR)/$$i; \
		$(SILENT) $(LIPO) -create \
			-arch ppc $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/ppc/$(USRBIN_DIR)/$$i  \
			-arch i386 $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/i386/$(USRBIN_DIR)/$$i  \
			-output $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRBIN_DIR)/$$i; \
		$(SILENT) $(STRIP) $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRBIN_DIR)/$$i > /dev/null 2>&1; \
		$(SILENT) $(LIPO) -info $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRBIN_DIR)/$$i; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: ...building - creating universal libraries"
	@echo "#"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRLIB_DIR)/gettext
	for i in $(GETTEXT_LIB_FILES_TO_LIPO); do \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRLIB_DIR)/$$i; \
		$(SILENT) $(LIPO) -create \
			-arch ppc $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/ppc/$(USRLIB_DIR)/$$i  \
			-arch i386 $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/i386/$(USRLIB_DIR)/$$i  \
			-output $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRLIB_DIR)/$$i; \
		$(SILENT) $(STRIP) $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRLIB_DIR)/$$i > /dev/null 2>&1; \
		$(SILENT) $(LIPO) -info $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRLIB_DIR)/$$i; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: ...building - caching includes"
	@echo "#"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/usr
	$(SILENT) $(DITTO) $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/ppc/usr/include $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/usr/include

gettext/build: $(GETTEXT_NAME)/config.status $(GETTEXT_NAME)/gettext

gettext/clean:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: cleaning sources"
	@echo "#"
	$(SILENT) $(RM) -rf $(GETTEXT_VERSION) $(GETTEXT_NAME) $(OBJROOT)/$(GETTEXT_BUILD_DIR)
	$(SILENT) $(TAR) -xzf $(GETTEXT_VERSION).tgz; $(MV) $(GETTEXT_VERSION) $(GETTEXT_NAME)

gettext/cleanobj:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: cleaning objects"
	@echo "#"
	for i in $(GETTEXT_ARCHS) ; do \
		$(SILENT) $(CD) $(OBJROOT)/$(GETTEXT_SRC_DIR)/$$i; make clean; \
	done

gettext/clean-all:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [gettext]: cleaning ALL"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(GETTEXT_BUILD_DIR)


#-------------------------------
# GLIB MODULE
#-------------------------------
#
# NOTE: There is no install target for glib.  It's make output is copied 
#       to $(OBJROOT)/$(GLIB_BUILD_DIR) so that the mu-conference target can 
#       use the library during the build, but the intermediate build results are 
#       discarded after the build completes.
#
# DIRECTORIES:
#       Sources:
#          tar output: $(SRCROOT)/$(GLIB_NAME)/
#          make sources (ppc): $(OBJROOT)/$(GLIB_SRC_DIR)/ppc/
#          make sources (i386): $(OBJROOT)/$(GLIB_SRC_DIR)/i386/
#       Build Products:
#          /usr/lib/* (ppc): $(OBJROOT)/$(GLIB_INSTALL_DIR)/ppc/$(USRLIB_DIR)
#          /usr/lib/* (i386): $(OBJROOT)/$(GLIB_INSTALL_DIR)/i386/$(USRLIB_DIR)
#          /usr/lib/* (universal): $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR)
#

GLIB_VERSION	= glib-2.10.0
GLIB_NAME	= glib

GLIB_BUILD_DIR	:= glib.build
GLIB_SRC_DIR	:= $(GLIB_BUILD_DIR)/src
GLIB_INSTALL_DIR	:= $(GLIB_BUILD_DIR)/install

# GLIB_BIN_FILES_TO_LIPO  List of all thin binaries that need to be lipo'd 
#                         into universal binaries for the glib component.
GLIB_BIN_FILES_TO_LIPO	= \
glib-genmarshal \
gobject-query

# GLIB_LIB_FILES_TO_LIPO  List of all thin libraries that need to be lipo'd 
#                         into universal libraries for the glib component.
GLIB_LIB_FILES_TO_LIPO	= \
libglib-2.0.a \
libgmodule-2.0.a \
libgobject-2.0.a \
libgthread-2.0.a

# GLIB_PC_FILES_TO_COPY   List of all glib library .pc to be copied into the
#                         /usr/lib directory for the glib component.
GLIB_PC_FILES_TO_COPY	= \
glib-2.0.pc \
gmodule-2.0.pc \
gobject-2.0.pc \
gthread-2.0.pc

GLIB_CONFIG_CFLAGS := -I$(OBJROOT)/$(GETTEXT_INSTALL_DIR)/usr/include
GLIB_CONFIG_LDFLAGS := -L$(OBJROOT)/$(GETTEXT_INSTALL_DIR)/usr/lib -L/System/Library/Frameworks/CoreFoundation.framework -L/usr/lib -framework CoreServices

GLIB_MAKE_CPPFLAGS := $(GLIB_CONFIG_CFLAGS)
GLIB_MAKE_CFLAGS := $(GLIB_CONFIG_CFLAGS)
GLIB_MAKE_LDLAGS := $(GLIB_CONFIG_LDFLAGS)

GLIB_ARCHS := ppc i386

export PATH := $(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRBIN_DIR):$(OBJROOT)/$(PKG_CONFIG_BIN_DIR):$(PATH)

# glib build rules
#
.PHONY: glib/untar glib/build glib/clean glib/cleanobj glib/clean-all

$(GLIB_NAME): $(OBJROOT)
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: UNPACKING"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(GLIB_SRC_DIR); $(MKDIR) -p -m 755 $(OBJROOT)/$(GLIB_SRC_DIR)
	for i in $(GLIB_ARCHS); do \
		$(SILENT) $(CD) $(OBJROOT)/$(GLIB_SRC_DIR); $(TAR) -xzf $(SRCROOT)/$(GLIB_VERSION).tgz; $(MV) $(GLIB_VERSION) $$i ;\
	done

glib/untar: $(GLIB_NAME)

# A NOTE ABOUT GLIB-GENMARSHAL:
#    Note that the GLIB_GENMARSHAL variable is always set to the path of the ppc version 
#    of the glib-genmarshal. The reason for this is due to limitations in cross-compiling
#    on PowerPC - i.e. when cross-compiling on ppc for i386, make cannot execute the 
#    i386 version of the tool, HOWEVER, thanks to Rosetta, cross-compiling on i386 for ppc,
#    make is able to run the ppc version of the tool.
#
# IMPORTANT: 
#    Because of the above glib-genmarshal restriction, building the glib target on a ppc host 
#    requires that the ppc architecture must always be built BEFORE the i386 architecture.
#
$(GLIB_NAME)/config.status: glib/untar
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: CONFIGURING"
	@echo "#"
	for i in $(GLIB_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: ...configuring; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CP) $(SRCROOT)/config_cache/glib/$$i/config.cache $(OBJROOT)/$(GLIB_SRC_DIR)/$$i/config.cache; \
		$(SILENT) $(CD) $(OBJROOT)/$(GLIB_SRC_DIR)/$$i; \
		env CFLAGS="-arch $$i $(GLIB_CONFIG_CFLAGS)" \
		    CPPFLAGS="-arch $$i $(GLIB_CONFIG_CFLAGS)" \
			LDFLAGS="-arch $$i $(GLIB_CONFIG_LDFLAGS)" \
			GLIB_GENMARSHAL="$(OBJROOT)/$(GLIB_SRC_DIR)/ppc/gobject/glib-genmarshal" \
		./configure --prefix=$(BUILD_DIR) --host=$$i-apple-darwin --enable-static --disable-shared --config-cache; \
	done

$(GLIB_NAME)/glib:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: BUILDING"
	@echo "#"
	for i in $(GLIB_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: ...building; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(GLIB_SRC_DIR)/$$i; \
		env CPPFLAGS="-arch $$i $(GLIB_MAKE_CPPFLAGS)" \
	         CFLAGS="-arch $$i $(GLIB_MAKE_CFLAGS)" \
		     LDFLAGS="-arch $$i $(GLIB_MAKE_LDLAGS)" \
		make; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: INSTALLING"
	@echo "#"
	for i in $(GLIB_ARCHS); do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: ...installing - caching make results; arch=$$i"; \
		$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(GLIB_INSTALL_DIR)/$$i; \
		$(SILENT) $(CD) $(OBJROOT)/$(GLIB_SRC_DIR)/$$i; make install DESTDIR=$(OBJROOT)/$(GLIB_INSTALL_DIR)/$$i; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: ...installing - creating universal binaries"
	@echo "#"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRBIN_DIR)
	for i in $(GLIB_BIN_FILES_TO_LIPO); do \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRBIN_DIR)/$$i; \
		$(SILENT) $(LIPO) -create \
			-arch ppc $(OBJROOT)/$(GLIB_INSTALL_DIR)/ppc/$(USRBIN_DIR)/$$i  \
			-arch i386 $(OBJROOT)/$(GLIB_INSTALL_DIR)/i386/$(USRBIN_DIR)/$$i  \
			-output $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRBIN_DIR)/$$i; \
		$(SILENT) $(STRIP) $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRBIN_DIR)/$$i > /dev/null 2>&1; \
		$(SILENT) $(LIPO) -info $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRBIN_DIR)/$$i; \
	done
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR)
	for i in $(GLIB_LIB_FILES_TO_LIPO); do \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR)/$$i; \
		$(SILENT) $(LIPO) -create \
			-arch ppc $(OBJROOT)/$(GLIB_INSTALL_DIR)/ppc/$(USRLIB_DIR)/$$i  \
			-arch i386 $(OBJROOT)/$(GLIB_INSTALL_DIR)/i386/$(USRLIB_DIR)/$$i  \
			-output $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR)/$$i; \
		$(SILENT) $(STRIP) $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR)/$$i > /dev/null 2>&1; \
		$(SILENT) $(LIPO) -info $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR)/$$i; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: ...installing - copying .pc files"
	@echo "#"
	for i in $(GLIB_PC_FILES_TO_COPY); do \
		$(SILENT) $(CP) $(OBJROOT)/$(GLIB_SRC_DIR)/ppc/$$i $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR)/; \
	done
	$(SILENT) $(LS) -l $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR)/*.pc
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: ...installing - caching includes"
	@echo "#"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(GLIB_INSTALL_DIR)/usr
	$(SILENT) $(DITTO) $(OBJROOT)/$(GLIB_INSTALL_DIR)/ppc/usr/include $(OBJROOT)/$(GLIB_INSTALL_DIR)/usr/include

glib/build: $(GLIB_NAME)/config.status $(GLIB_NAME)/glib

glib/clean:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: cleaning sources"
	@echo "#"
	$(SILENT) $(RM) -rf $(GLIB_VERSION) $(GLIB_NAME) $(OBJROOT)/$(GLIB_BUILD_DIR)
	$(SILENT) $(TAR) -xzf $(GLIB_VERSION).tgz; $(MV) $(GLIB_VERSION) $(GLIB_NAME)

glib/cleanobj:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: cleaning objects"
	@echo "#"
	for i in $(GLIB_ARCHS); do \
		$(SILENT) $(CD) $(OBJROOT)/$(GLIB_SRC_DIR)/$$i; make clean; \
	done

glib/clean-all:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [glib]: cleaning ALL"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(GLIB_BUILD_DIR)


#-------------------------------
# MU-CONFERENCE MODULE
#-------------------------------
#
# NOTE: The mu-conference module is a libary that is installed in (WHERE?).  
#       During the muc/build phase, the make output is copied to 
#       $(OBJROOT)/$(MUC_BUILD_DIR) in preparation for final installation.
#
# DIRECTORIES:
#       Sources:
#          tar output: $(SRCROOT)/$(MUC_NAME)/
#          make sources (ppc): $(OBJROOT)/$(MUC_SRC_DIR)/ppc/
#          make sources (i386): $(OBJROOT)/$(MUC_SRC_DIR)/i386/
#       Build Products:
#          /usr/lib/* (ppc): $(OBJROOT)/$(MUC_INSTALL_DIR)/ppc/$(USRLIB_DIR)
#          /usr/lib/* (i386): $(OBJROOT)/$(MUC_INSTALL_DIR)/i386/$(USRLIB_DIR)
#          /usr/lib/* (universal): $(OBJROOT)/$(MUC_INSTALL_DIR)/$(USRLIB_DIR)
#

MUC_VERSION	= mu-conference-0.6.0
MUC_NAME	= mu-conference

JCR_VERSION	= jcr-0.2.4
JCR_NAME	= jcr

MUC_BUILD_DIR	:= mu-conference.build
MUC_SRC_DIR	:= $(MUC_BUILD_DIR)/src
MUC_INSTALL_DIR	:= $(MUC_BUILD_DIR)/install

# MUC_BIN_FILES_TO_LIPO  List of all thin binaries that need to be lipo'd 
#                         into universal binaries for the glib component.
MUC_BIN_FILES_TO_LIPO	= mu-conference

# MUC_LIB_FILES_TO_LIPO  List of all thin libraries that need to be lipo'd 
#                         into universal libraries for the glib component.
MUC_LIB_FILES_TO_LIPO	= \
libglib-2.0.a \
libgmodule-2.0.a \
libgobject-2.0.a \
libgthread-2.0.a

JCR_MAKE_CFLAGS := -I$(OBJROOT)/$(MUC_SRC_DIR)/ppc/$(JCR_NAME)/lib \
                   -I$(OBJROOT)/$(GLIB_INSTALL_DIR)/usr/include/glib-2.0/glib \
                   -I$(OBJROOT)/$(GLIB_INSTALL_DIR)/usr/include/glib-2.0 \
                   -I$(OBJROOT)/$(GLIB_SRC_DIR)/ppc

MUC_MAKE_CFLAGS := -I$(OBJROOT)/$(MUC_SRC_DIR)/ppc/$(JCR_NAME)/$(MUC_NAME)/include \
                   -I$(OBJROOT)/$(MUC_SRC_DIR)/ppc/$(JCR_NAME)/lib \
                   -I$(OBJROOT)/$(GLIB_INSTALL_DIR)/usr/include/glib-2.0/glib \
                   -I$(OBJROOT)/$(GLIB_INSTALL_DIR)/usr/include/glib-2.0 \
                   -I$(OBJROOT)/$(GLIB_SRC_DIR)/ppc
MUC_MAKE_LDFLAGS := -L$(OBJROOT)/$(MUC_SRC_DIR)/$$i/$(JCR_NAME)/lib -L$(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR) -L$(OBJROOT)/$(GETTEXT_INSTALL_DIR)/$(USRLIB_DIR) -framework CoreFoundation
MUC_MAKE_PKG_CONFIG_PATH := $(OBJROOT)/$(GLIB_INSTALL_DIR)/$(USRLIB_DIR):$(PATH)

MUC_ARCHS := ppc i386

# mu-conference build rules
#
.PHONY: muc/untar muc/jcr muc/muc muc/install muc/build muc/clean muc/cleanobj muc/clean-all

$(MUC_NAME): $(OBJROOT)
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [mu-conference]: UNPACKING"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(MUC_SRC_DIR)
	for i in $(MUC_ARCHS); do \
		$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(MUC_SRC_DIR)/$$i; \
		$(SILENT) $(CD) $(OBJROOT)/$(MUC_SRC_DIR)/$$i;$(TAR) -xzf $(SRCROOT)/$(JCR_VERSION).tgz; $(MV) $(JCR_VERSION) $(JCR_NAME); $(CHOWN) -R $(USER):$(GROUP) $(JCR_NAME); \
		$(SILENT) $(CD) $(OBJROOT)/$(MUC_SRC_DIR)/$$i/$(JCR_NAME); $(TAR) -xzf $(SRCROOT)/$(MUC_VERSION).tgz; $(MV) $(MUC_VERSION) $(MUC_NAME); $(CHOWN) -R $(USER):$(GROUP) $(MUC_NAME); \
	done

muc/untar: $(MUC_NAME)

muc/jcr:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [muc-jcr]: BUILDING"
	@echo "#"
	for i in $(MUC_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [muc-jcr]: ...building; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(MUC_SRC_DIR)/$$i/$(JCR_NAME); make CFLAGS="-arch $$i $(JCR_MAKE_CFLAGS)"; \
	done

muc/muc:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [mu-conference]: BUILDING"
	@echo "#"
	for i in $(MUC_ARCHS); do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [mu-conference]: ...building -- merging JCR sources; arch=$$i; "; \
		$(CP) $(OBJROOT)/$(MUC_SRC_DIR)/$$i/$(JCR_NAME)/src/main.c $(OBJROOT)/$(MUC_SRC_DIR)/$$i/$(JCR_NAME)/$(MUC_NAME)/src/; \
		$(CP) $(OBJROOT)/$(MUC_SRC_DIR)/$$i/$(JCR_NAME)/src/jcomp.mk $(OBJROOT)/$(MUC_SRC_DIR)/$$i/$(JCR_NAME)/$(MUC_NAME)/src/; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [mu-conference]: ...building; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(MUC_SRC_DIR)/$$i/$(JCR_NAME)/$(MUC_NAME)/src; \
		make -f jcomp.mk PKG_CONFIG_PATH=$(MUC_MAKE_PKG_CONFIG_PATH) \
		                 CFLAGS="-arch $$i $(MUC_MAKE_CFLAGS)" \
		                 LDFLAGS="-arch $$i $(MUC_MAKE_LDFLAGS)"; \
	done

muc/install:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [mu-conference]: INSTALLING"
	@echo "#"
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [mu-conference]: ...installing - creating universal binaries"
	@echo "#"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(MUC_INSTALL_DIR)/$(LIBEXEC_DIR)
	for i in $(MUC_BIN_FILES_TO_LIPO); do \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(MUC_INSTALL_DIR)/$(LIBEXEC_DIR)/$$i; \
		$(SILENT) $(LIPO) -create \
			-arch ppc $(OBJROOT)/$(MUC_SRC_DIR)/ppc/$(JCR_NAME)/$(MUC_NAME)/src/$$i  \
			-arch i386 $(OBJROOT)/$(MUC_SRC_DIR)/i386/$(JCR_NAME)/$(MUC_NAME)/src/$$i  \
			-output $(OBJROOT)/$(MUC_INSTALL_DIR)/$(LIBEXEC_DIR)/$$i; \
		$(SILENT) $(CP) $(OBJROOT)/$(MUC_INSTALL_DIR)/$(LIBEXEC_DIR)/$$i $(SYMROOT)/ > /dev/null 2>&1; \
		$(SILENT) $(STRIP) $(OBJROOT)/$(MUC_INSTALL_DIR)/$(LIBEXEC_DIR)/$$i > /dev/null 2>&1; \
		$(SILENT) $(LIPO) -info $(OBJROOT)/$(MUC_INSTALL_DIR)/$(LIBEXEC_DIR)/$$i; \
	done

muc/build: muc/untar muc/jcr muc/muc muc/install

muc/clean:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [mu-conference]: cleaning sources"
	@echo "#"
	$(SILENT) $(RM) -rf $(MUC_VERSION) $(MUC_NAME) $(OBJROOT)/$(MUC_BUILD_DIR)
	$(SILENT) $(TAR) -xzf $(MUC_VERSION).tgz; $(MV) $(MUC_VERSION) $(MUC_NAME)

muc/cleanobj:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [mu-conference]: cleaning objects"
	@echo "#"
	for i in $(MUC_ARCHS); do \
		$(SILENT) $(CD) $(OBJROOT)/$(MUC_SRC_DIR)/$$i; make clean; \
	done

muc/clean-all:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [mu-conference]: cleaning ALL"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(MUC_BUILD_DIR)

#-------------------------------
# JABBER_OD_AUTH MODULE
#-------------------------------
#
# NOTE: There is no install target for jabber_od_auth.  It's make output is built 
#       into $(OBJROOT)/$(STAGING_DIR)/$(ODAUTH_LIB_DIR) so that the jabber target can 
#       use this library during it's own build phase, then discarded after the 
#       build is finished.
#
# DIRECTORIES:
#       Sources:
#          make sources (universal): $(SRCROOT)/$(ODAUTH_SRC_DIR)/
#       Build Products:
#          objects (ppc):        $(OBJROOT)/$(ODAUTH_BUILD_DIR)/ppc/
#          objects (i386):       $(OBJROOT)/$(ODAUTH_BUILD_DIR)/i386/
#          library (ppc):        $(OBJROOT)/$(ODAUTH_BUILD_DIR)/ppc/
#          library (i386):       $(OBJROOT)/$(ODAUTH_BUILD_DIR)/i386/
#          library (universal):  $(OBJROOT)/$(ODAUTH_LIB_DIR)/
#          includes (universal): $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/
#

ODAUTH_SRC_DIR	:= jabber_od_auth
ODAUTH_BUILD_DIR	:= jabber_od_auth.build
ODAUTH_LIB_DIR	:= $(ODAUTH_BUILD_DIR)/lib
ODAUTH_INCLUDE_DIR	:= $(ODAUTH_BUILD_DIR)/include

OD_AUTH_INCLUDE_PATHS = \
-I/System/Library/Frameworks/CoreFoundation.framework/Headers \
-I/System/Library/Frameworks/DirectoryService.framework/Headers \
-I/System/Library/Frameworks/CoreServices.framework/Headers \
-I/System/Library/Frameworks/Security.framework/PrivateHeaders \
-I/System/Library/Frameworks/System.framework/PrivateHeaders \
-I/usr/include

OD_AUTH_FRAMEWORK_SEARCH_PATHS = \
-F/System/Library/Frameworks \
-F/System/Library/PrivateFrameworks

OD_AUTH_FRAMEWORKS = \
-framework CoreServices \
-framework DirectoryService \
-framework Security \
-framework System

OD_AUTH_LIB_SEARCH_PATHS = -L/usr/lib
OD_AUTH_LINK_LIBRARIES = /usr/lib/libcrypto.dylib
OD_AUTH_SYSLIBROOT = /Developer/SDKs/MacOSX10.5.sdk

OD_AUTH_ARCHS := ppc i386

#
# od_auth/build rules
#   od_auth/build       : builds the jabber_od_auth library
#
.PHONY: od_auth/build

od_auth/build:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabber_od_auth]: BUILDING; DEST=$(OBJROOT)/$(ODAUTH_BUILD_DIR)"
	@echo "#"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(ODAUTH_LIB_DIR)
	for i in $(OD_AUTH_ARCHS); do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabber_od_auth]: ...building; arch=$$i"; \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(ODAUTH_BUILD_DIR)/$$i; \
		$(SILENT) $(MKDIR) -p -m 777 $(OBJROOT)/$(ODAUTH_BUILD_DIR)/$$i; \
		$(SILENT) $(GCC) -g -O2 -arch $$i -c $(OD_AUTH_INCLUDE_PATHS) \
	                 $(ODAUTH_SRC_DIR)/apple_authenticate.c \
	                 -o $(OBJROOT)/$(ODAUTH_BUILD_DIR)/$$i/apple_authenticate.o; \
		$(SILENT) $(GCC) -g -O2 -arch $$i -c $(OD_AUTH_INCLUDE_PATHS) \
					 $(ODAUTH_SRC_DIR)/apple_authorize.c \
	                 -o $(OBJROOT)/$(ODAUTH_BUILD_DIR)/$$i/apple_authorize.o; \
		$(SILENT) $(APPLELIBTOOL) -static -arch_only $$i \
	                          -o $(OBJROOT)/$(ODAUTH_BUILD_DIR)/$$i/libxmppodauth.a \
						      $(OD_AUTH_FRAMEWORK_SEARCH_PATHS) $(OD_AUTH_LIB_SEARCH_PATHS) \
						      $(OD_AUTH_FRAMEWORKS) $(OD_AUTH_LINK_LIBRARIES) \
						      $(OBJROOT)/$(ODAUTH_BUILD_DIR)/$$i/apple_authenticate.o \
						      $(OBJROOT)/$(ODAUTH_BUILD_DIR)/$$i/apple_authorize.o \
						      -syslibroot $(OD_AUTH_SYSLIBROOT); \
	done
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabber_od_auth]: ...building - creating universal library"
	$(SILENT) $(LIPO) -create \
		-arch ppc $(OBJROOT)/$(ODAUTH_BUILD_DIR)/ppc/libxmppodauth.a  \
		-arch i386 $(OBJROOT)/$(ODAUTH_BUILD_DIR)/i386/libxmppodauth.a   \
		-output $(OBJROOT)/$(ODAUTH_LIB_DIR)/libxmppodauth.a
	$(SILENT) $(LIPO) -info $(OBJROOT)/$(ODAUTH_LIB_DIR)/libxmppodauth.a
	$(SILENT) $(CP) $(ODAUTH_SRC_DIR)/apple_authenticate.h $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/
	$(SILENT) $(CP) $(ODAUTH_SRC_DIR)/apple_authorize.h $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/

od_auth/clean:
	$(SILENT) $(RM) -rf $(OBJROOT)/$(ODAUTH_BUILD_DIR)


#-------------------------------
# JABBER_AUTOBUDDY MODULE
#-------------------------------
#
# NOTE: There is no install target for jabber_autobuddy in this build phase.  
#       It's make output is built into $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/bin 
#       then later copied to it's final installation folder during execution of 
#       the install target (see below).
#
# DIRECTORIES:
#       Sources:
#          make sources (universal): $(SRCROOT)/$(AUTOBUDDY_SRC_DIR)/
#       Build Products:
#          tool (ppc): $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/ppc/
#          tool (i386): $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/i386/
#          tool universal: $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/bin/
#
#

AUTOBUDDY_SRC_DIR	:= jabber_autobuddy
AUTOBUDDY_BUILD_DIR	:= autobuddy.build

AUTOBUDDY_ARCHS := ppc i386

# jabber_autobuddy build rules
#

.PHONY: autobuddy/build

#
# autobuddy/build rules
#   autobuddy/build       : builds the jabber_autobuddy tool
#
autobuddy/build:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabber_autobuddy]: BUILDING; DEST=$(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)"
	@echo "#"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/bin
	for i in $(OD_AUTH_ARCHS); do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabber_autobuddy]: ...building; arch=$$i"; \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/$$i; \
		$(SILENT) $(MKDIR) -p -m 777 $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/$$i; \
		$(SILENT) $(GCC) -g -O2 -arch $$i \
	                 -o $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/$$i/jabber_autobuddy \
					 -lsqlite3 $(AUTOBUDDY_SRC_DIR)/jabber_autobuddy.c; \
	done
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabber_autobuddy]: ...building - creating universal executable"
	$(SILENT) $(LIPO) -create \
		-arch ppc $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/ppc/jabber_autobuddy  \
		-arch i386 $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/i386/jabber_autobuddy   \
		-output $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/bin/jabber_autobuddy
	$(SILENT) $(STRIP) $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/bin/jabber_autobuddy > /dev/null 2>&1
	$(SILENT) $(LIPO) -info $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/bin/jabber_autobuddy

autobuddy/clean:
	$(SILENT) $(RM) -rf $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)


#-------------------------------
# JABBERD2 MODULE
#-------------------------------
#
JABBERD2_VERSION	= jabberd-2.0s9-patched-configured-fix1
JABBERD2_SAVE	= jabberd2-save
JABBERD2_NAME	= jabberd2

.PHONY: jabberd2/jabberd2 jabberd2/makeinstall jabberd2/build jabberd2/clean jabberd2/cleanobj jabberd2/clean-all

JABBERD2_BUILD_DIR 	:= jabberd2.build
JABBERD2_SRC_DIR	:= $(JABBERD2_BUILD_DIR)/src
JABBERD2_INSTALL_DIR	:= $(JABBERD2_BUILD_DIR)/install

JABBERD2_ARCHS 	:= ppc i386

JABBERD2_CONFIG_OPTS= \
		--infodir=/$(INFO_DIR) \
		--mandir=/$(MAN_DIR) \
		--sysconfdir=/$(SYSCONFIG_DIR) \
		--disable-mysql \
		--enable-sqlite \
		--enable-debug \
		--with-extra-include-path=$(OBJROOT)/$(LIBIDN_INCLUDE_DIR):$(SASL_INCLUDE_DIR):$(SQLITE_INCLUDE_DIR):$(OBJROOT)/$(ODAUTH_INCLUDE_DIR) \
		--with-extra-library-path=$(OBJROOT)/$(LIBIDN_LIB_DIR):$(MYSQL_LIB_DIR):$(SQLITE_LIB_DIR):$(OBJROOT)/$(ODAUTH_LIB_DIR)

# JABBERD2_FILES_TO_LIPO  List of all thin binaries that need to be lipo'd 
#                         into universal binaries for the jabberd2 component.
JABBERD2_FILES_TO_LIPO	= c2s resolver router s2s sm
# JABBERD2_FILES_TO_ARCHIVE  All files archived by the jabberd2/cleanobj target.
#                            WARNING: any files introduced into the jabberd2
#                            source folders must be added here too to avoid
#                            losing changes which are not committed to CVS.
JABBERD2_FILES_TO_ARCHIVE	= \
jabberd2/ac-stdint.h \
jabberd2/acinclude.m4 \
jabberd2/aclocal.m4 \
jabberd2/AUTHORS \
jabberd2/autom4te.cache/output.0 \
jabberd2/autom4te.cache/requests \
jabberd2/autom4te.cache/traces.0 \
jabberd2/c2s/authreg.c \
jabberd2/c2s/authreg_anon.c \
jabberd2/c2s/authreg_db.c \
jabberd2/c2s/authreg_ldap.c \
jabberd2/c2s/authreg_mysql.c \
jabberd2/c2s/authreg_pam.c \
jabberd2/c2s/authreg_pgsql.c \
jabberd2/c2s/authreg_pipe.c \
jabberd2/c2s/authreg_sqlite.c \
jabberd2/c2s/bind.c \
jabberd2/c2s/c2s.c \
jabberd2/c2s/c2s.h \
jabberd2/c2s/main.c \
jabberd2/c2s/Makefile \
jabberd2/c2s/Makefile.am \
jabberd2/c2s/Makefile.in \
jabberd2/c2s/sm.c \
jabberd2/ChangeLog \
jabberd2/config.guess \
jabberd2/config.h \
jabberd2/config.h.in \
jabberd2/config.log \
jabberd2/config.rpath \
jabberd2/config.status \
jabberd2/config.sub \
jabberd2/configure \
jabberd2/configure.in \
jabberd2/contrib/patch-flash-v3rc1 \
jabberd2/COPYING \
jabberd2/depcomp \
jabberd2/Doxyfile \
jabberd2/Doxyfile.in \
jabberd2/etc/c2s.xml.dist.in \
jabberd2/etc/jabberd.cfg.dist.in \
jabberd2/etc/Makefile \
jabberd2/etc/Makefile.am \
jabberd2/etc/Makefile.in \
jabberd2/etc/resolver.xml.dist.in \
jabberd2/etc/router-users.xml.dist.in \
jabberd2/etc/router.xml.dist.in \
jabberd2/etc/s2s.xml.dist.in \
jabberd2/etc/sm.xml.dist.in \
jabberd2/etc/templates/Makefile \
jabberd2/etc/templates/Makefile.am \
jabberd2/etc/templates/Makefile.in \
jabberd2/etc/templates/roster.xml.dist.in \
jabberd2/expat/ascii.h \
jabberd2/expat/asciitab.h \
jabberd2/expat/expat.h \
jabberd2/expat/expat_config.h \
jabberd2/expat/iasciitab.h \
jabberd2/expat/internal.h \
jabberd2/expat/latin1tab.h \
jabberd2/expat/Makefile \
jabberd2/expat/Makefile.am \
jabberd2/expat/Makefile.in \
jabberd2/expat/nametab.h \
jabberd2/expat/utf8tab.h \
jabberd2/expat/winconfig.h \
jabberd2/expat/xmlparse.c \
jabberd2/expat/xmlrole.c \
jabberd2/expat/xmlrole.h \
jabberd2/expat/xmltok.c \
jabberd2/expat/xmltok.h \
jabberd2/expat/xmltok_impl.c \
jabberd2/expat/xmltok_impl.h \
jabberd2/expat/xmltok_ns.c \
jabberd2/INSTALL \
jabberd2/install-sh \
jabberd2/libtool \
jabberd2/ltmain.sh \
jabberd2/Makefile \
jabberd2/Makefile.am \
jabberd2/Makefile.in \
jabberd2/man/c2s.8.in \
jabberd2/man/jabberd.8.in \
jabberd2/man/Makefile \
jabberd2/man/Makefile.am \
jabberd2/man/Makefile.in \
jabberd2/man/resolver.8.in \
jabberd2/man/router.8.in \
jabberd2/man/s2s.8.in \
jabberd2/man/sm.8.in \
jabberd2/mio/Makefile \
jabberd2/mio/Makefile.am \
jabberd2/mio/Makefile.in \
jabberd2/mio/mio.c \
jabberd2/mio/mio.h \
jabberd2/mio/mio_poll.h \
jabberd2/mio/mio_select.h \
jabberd2/missing \
jabberd2/NEWS \
jabberd2/PROTOCOL \
jabberd2/README \
jabberd2/README.win32 \
jabberd2/resolver/dns.c \
jabberd2/resolver/dns.h \
jabberd2/resolver/Makefile \
jabberd2/resolver/Makefile.am \
jabberd2/resolver/Makefile.in \
jabberd2/resolver/resolver.c \
jabberd2/resolver/resolver.h \
jabberd2/resolver/TODO \
jabberd2/router/aci.c \
jabberd2/router/main.c \
jabberd2/router/Makefile \
jabberd2/router/Makefile.am \
jabberd2/router/Makefile.in \
jabberd2/router/router.c \
jabberd2/router/router.h \
jabberd2/router/user.c \
jabberd2/s2s/in.c \
jabberd2/s2s/main.c \
jabberd2/s2s/Makefile \
jabberd2/s2s/Makefile.am \
jabberd2/s2s/Makefile.in \
jabberd2/s2s/out.c \
jabberd2/s2s/router.c \
jabberd2/s2s/s2s.h \
jabberd2/s2s/sx.c \
jabberd2/s2s/util.c \
jabberd2/scod/Makefile \
jabberd2/scod/Makefile.am \
jabberd2/scod/Makefile.in \
jabberd2/scod/mech_anonymous.c \
jabberd2/scod/mech_digest_md5.c \
jabberd2/scod/mech_plain.c \
jabberd2/scod/scod.c \
jabberd2/scod/scod.h \
jabberd2/sm/aci.c \
jabberd2/sm/dispatch.c \
jabberd2/sm/feature.c \
jabberd2/sm/main.c \
jabberd2/sm/Makefile \
jabberd2/sm/Makefile.am \
jabberd2/sm/Makefile.in \
jabberd2/sm/mm.c \
jabberd2/sm/mod_active.c \
jabberd2/sm/mod_announce.c \
jabberd2/sm/mod_deliver.c \
jabberd2/sm/mod_disco.c \
jabberd2/sm/mod_disco_publish.c \
jabberd2/sm/mod_echo.c \
jabberd2/sm/mod_help.c \
jabberd2/sm/mod_iq_last.c \
jabberd2/sm/mod_iq_private.c \
jabberd2/sm/mod_iq_time.c \
jabberd2/sm/mod_iq_vcard.c \
jabberd2/sm/mod_iq_version.c \
jabberd2/sm/mod_offline.c \
jabberd2/sm/mod_presence.c \
jabberd2/sm/mod_privacy.c \
jabberd2/sm/mod_roster.c \
jabberd2/sm/mod_session.c \
jabberd2/sm/mod_template_roster.c \
jabberd2/sm/mod_vacation.c \
jabberd2/sm/mod_validate.c \
jabberd2/sm/object.c \
jabberd2/sm/pkt.c \
jabberd2/sm/pres.c \
jabberd2/sm/sess.c \
jabberd2/sm/sm.c \
jabberd2/sm/sm.h \
jabberd2/sm/storage.c \
jabberd2/sm/storage_db.c \
jabberd2/sm/storage_fs.c \
jabberd2/sm/storage_mysql.c \
jabberd2/sm/storage_oracle.c \
jabberd2/sm/storage_pgsql.c \
jabberd2/sm/storage_sqlite.c \
jabberd2/sm/user.c \
jabberd2/stamp-h1 \
jabberd2/subst/dirent.c \
jabberd2/subst/dirent.h \
jabberd2/subst/getopt.c \
jabberd2/subst/getopt.h \
jabberd2/subst/gettimeofday.c \
jabberd2/subst/inet_aton.c \
jabberd2/subst/inet_ntop.c \
jabberd2/subst/inet_pton.c \
jabberd2/subst/ip6_misc.h \
jabberd2/subst/Makefile \
jabberd2/subst/Makefile.am \
jabberd2/subst/Makefile.in \
jabberd2/subst/snprintf.c \
jabberd2/subst/subst.h \
jabberd2/subst/syslog.c \
jabberd2/subst/syslog.h \
jabberd2/sx/callback.c \
jabberd2/sx/chain.c \
jabberd2/sx/client.c \
jabberd2/sx/env.c \
jabberd2/sx/error.c \
jabberd2/sx/io.c \
jabberd2/sx/Makefile \
jabberd2/sx/Makefile.am \
jabberd2/sx/Makefile.in \
jabberd2/sx/sasl.c \
jabberd2/sx/sasl.h \
jabberd2/sx/server.c \
jabberd2/sx/ssl.c \
jabberd2/sx/ssl.h \
jabberd2/sx/sx.c \
jabberd2/sx/sx.h \
jabberd2/TODO \
jabberd2/tools/db-setup.oracle \
jabberd2/tools/db-setup.pgsql \
jabberd2/tools/db-setup.sqlite \
jabberd2/tools/db-update.mysql \
jabberd2/tools/jabberd.in \
jabberd2/tools/jabberd.rc \
jabberd2/tools/Makefile \
jabberd2/tools/Makefile.am \
jabberd2/tools/Makefile.in \
jabberd2/tools/migrate.pl \
jabberd2/tools/pipe-auth.pl \
jabberd2/util/access.c \
jabberd2/util/base64.c \
jabberd2/util/config.c \
jabberd2/util/datetime.c \
jabberd2/util/hex.c \
jabberd2/util/inaddr.c \
jabberd2/util/inaddr.h \
jabberd2/util/jid.c \
jabberd2/util/jqueue.c \
jabberd2/util/jsignal.c \
jabberd2/util/log.c \
jabberd2/util/Makefile \
jabberd2/util/Makefile.am \
jabberd2/util/Makefile.in \
jabberd2/util/md5.c \
jabberd2/util/md5.h \
jabberd2/util/nad.c \
jabberd2/util/pool.c \
jabberd2/util/rate.c \
jabberd2/util/serial.c \
jabberd2/util/sha1.c \
jabberd2/util/sha1.h \
jabberd2/util/stanza.c \
jabberd2/util/str.c \
jabberd2/util/util.h \
jabberd2/util/util_compat.h \
jabberd2/util/xdata.c \
jabberd2/util/xdata.h \
jabberd2/util/xhash.c

# jabberd2 build rules
#
#   jabberd2/config.status  : runs the configure script for the jabberd2 sources
#   jabberd2/build          : runs make for the jabberd2 sources to produce the 
#                             intermediate binaries and other build products
#   jabberd2/makeinstall    : creates installed file and directoried in an intermediate directory
#   jabberd2/build          : wrapper rule for config/build/makeinstall
#

$(JABBERD2_NAME)/config.status:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: CONFIGURING; SRCROOT=$(SRCROOT)"
	@echo "#"
	for i in $(JABBERD2_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - SRCROOT=$(SRCROOT)"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - creating $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i"; \
		$(SILENT) $(MKDIR) -p -m 777 $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; \
		$(SILENT) $(DITTO) $(SRCROOT)/$(JABBERD2_NAME) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; $(RM) -rf ac-stdint.h; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - patching $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i/ltmain.sh; arch=$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; $(PATCH) -u ltmain.sh $(SRCROOT)/ltmain.sh.patch; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - creating final sources; arch=$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; \
		./configure --prefix=$(BUILD_DIR) --target=$$i-apple-darwin $(JABBERD2_CONFIG_OPTS); \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - patching $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i/Makefile; arch=$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; $(CP) Makefile Makefile.bak; $(PATCH) -u Makefile $(SRCROOT)/mfpatch/jabberd2/Makefile.patch; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - patching $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i/c2s/Makefile; arch=$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i/c2s; $(CP) Makefile Makefile.bak; $(PATCH) -u Makefile $(SRCROOT)/mfpatch/jabberd2/c2s/Makefile.patch; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - patching $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i/resolver/Makefile; arch=$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i/resolver; $(CP) Makefile Makefile.bak; $(PATCH) -u Makefile $(SRCROOT)/mfpatch/jabberd2/resolver/Makefile.patch; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - patching $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i/config.h; arch=$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; $(CP) config.h config.h.bak; $(PATCH) -u config.h $(SRCROOT)/config_h.patch; \
	done

jabberd2/jabberd2: $(JABBERD2_NAME)/config.status
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: BUILDING; SRCROOT=$(SRCROOT)"
	@echo "#"
	for i in $(JABBERD2_ARCHS); do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...building; arch=$$i; src=$(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; \
		make CFLAGS="-D HAVE_MEMMOVE -g -O2 -arch $$i" LDFLAGS="-framework Security"; \
	done

jabberd2/makeinstall:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: INSTALLING; DESTDIR=$(OBJROOT)/$(JABBERD2_BUILD_DIR)"
	@echo "#"
	for i in $(JABBERD2_ARCHS); do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...installing; arch=$$i"; \
		$(SILENT) $(MKDIR) -p -m 777 $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$$i; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; make install DESTDIR=$(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$$i; \
	done
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...installing - creating universal binaries"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRBIN_DIR)
	$(SILENT) $(MKDIRS) $(SYMROOT)/$(USRBIN_DIR)
	for i in $(JABBERD2_FILES_TO_LIPO); do \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRBIN_DIR)/$$i; \
		$(SILENT) $(LIPO) -create \
			-arch ppc $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/ppc/$(USRBIN_DIR)/$$i  \
			-arch i386 $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/i386/$(USRBIN_DIR)/$$i  \
			-output $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRBIN_DIR)/$$i; \
		$(SILENT) $(CP) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRBIN_DIR)/$$i $(SYMROOT)/$(USRBIN_DIR)/; \
		$(SILENT) $(STRIP) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRBIN_DIR)/$$i > /dev/null 2>&1; \
		$(SILENT) $(LIPO) -info $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRBIN_DIR)/$$i; \
	done
	#@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...installing - patching jabberd script"
	#$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/ppc/$(USRBIN_DIR); $(PATCH) -u jabberd $(SRCROOT)/jabberd.patch
	$(SILENT) $(INSTALL) -m 755 -o root -g wheel $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/ppc/$(USRBIN_DIR)/jabberd $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRBIN_DIR)
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...installing version and license info"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceVersions
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/jabberd2.plist $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceVersions
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceLicenses
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/jabberd2.txt $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceLicenses
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...installing - copying man pages for executables"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(MAN_DIR)
	$(SILENT) $(DITTO) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/ppc/$(MAN_DIR) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(MAN_DIR)

jabberd2/build: jabberd2/jabberd2 jabberd2/makeinstall

# jabberd2/clean rules
#   jabberd2/clean     : erase all except pre-defined jabberd2 sources (DEFAULT)
#   jabberd2/cleanobj  : standard make clean to erase just the object files
#   jabberd2/clean-all : wipe the jabberd2 directory for later restore from archive

jabberd2/clean:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: cleaning OBJECTS"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(JABBERD2_SRC_DIR)

jabberd2/cleanobj:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: cleaning SOURCES"
	@echo "#"
	$(SILENT) $(TAR) -czf $(JABBERD2_SAVE).tgz $(JABBERD2_FILES_TO_ARCHIVE)
	$(SILENT) -$(RM) -rf $(JABBERD2_VERSION) $(JABBERD2_NAME) $(OBJROOT)/$(JABBERD2_SRC_DIR)
	$(SILENT) $(TAR) -xzf $(JABBERD2_SAVE).tgz

jabberd2/clean-all:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: cleaning ALL"
	@echo "#"
	$(SILENT) $(RM) -rf $(JABBERD2_NAME) $(OBJROOT)/$(JABBERD2_SRC_DIR)

#-------------------------------
# TOP-LEVEL MAKE RULES
#-------------------------------
#
# DEPENDANCIES: 
#  1) jabberd2 requires libidn.
#  2) autobuddy requires jabberd2.
#

BACKUPRESTORE_SRC_DIR=backup_restore
MIGRATION_SRC_DIR=migration

# FILES_TO_INSTALL  Used by the installsrc target.
#                   WARNING! Make sure to update this will ALL project sources 
#                   (files AND folders), or the B&I build may fail!!!

FILES_TO_INSTALL		=  \
Makefile ichatserver_init_tool ichatserver_init_tool.8 \
config_h.patch jabberd.patch ltmain.sh.patch mfpatch cfg-apple config_cache \
$(AUTOBUDDY_SRC_DIR) $(BACKUPRESTORE_SRC_DIR) $(ODAUTH_SRC_DIR) \
$(MIGRATION_SRC_DIR) $(LIBIDN_VERSION).tgz libidn.plist libidn.txt \
$(PKG_CONFIG_VERSION).tgz pkg-config.plist pkg-config.txt \
$(GETTEXT_VERSION).tgz gettext.plist gettext.txt \
$(GLIB_VERSION).tgz glib.plist glib.txt \
$(MUC_VERSION).tgz mu-conference.plist mu-conference.txt \
mu-conference.8 $(JABBERD2_NAME) jabberd2.plist jabberd2.txt

.PHONY: all build configure install untar clean cleanobj clean-all installhdrs installsrc \
        install/intro_banner install/jabber_usr_dir install/autobuddy \
        install/mu-conference install/runtime_scripts install/man_pages \
		install/custom_configs install/sbsbackup install/jabber_var_dirs \
		install/file_proxy install/migration install/copy_dstroot

all: build

build: pkg-config/build gettext/build glib/build muc/build od_auth/build libidn/build jabberd2/build autobuddy/build

configure: $(LIBIDN_NAME)/config.status libidn/build $(JABBERD2_NAME)/config.status

#
# Create the final install image for DSTROOT
#
# The install target takes all the intermediate build products from jabberd2 and the other 
# components and creates STAGING_DIR, which is used to intall all the build products into
# their final directories and perform any final patches.  Once STAGING_DIR is fully
# processed, it is then copied to DSTROOT as the final set of files to be installed in the OS.
#
# - Executables are lipo'd from each installed architecture and combined into a single universal 
#   binary in the common staging directory.
# - Non-executables generated by the configure script are installed from the ppc build into the 
#   common staging directory.
# - Other files are copied from the common source directory into the common staging directory.
# - Miscellaneous patch are applied here before final installation in DSTROOT.
#

JABBER_VAR_DIR=$(VAR_DIR)/jabberd

#
# Install sub-targets
#

install/intro_banner:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: COPYING; STAGING_DIR=$(OBJROOT)/$(STAGING_DIR)"
	@echo "#"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(STAGING_DIR)/$(USRBIN_DIR)

install/jabber_usr_dir:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying jabber binaries"
	$(SILENT) $(DITTO) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/usr $(OBJROOT)/$(STAGING_DIR)/usr

install/autobuddy:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying autobuddy binary"
	$(SILENT) $(CP) $(OBJROOT)/$(AUTOBUDDY_BUILD_DIR)/bin/jabber_autobuddy $(OBJROOT)/$(STAGING_DIR)/$(USRBIN_DIR)/

install/mu-conference:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying mu-conference binary"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)
	$(SILENT) $(CP) $(OBJROOT)/$(MUC_INSTALL_DIR)/$(LIBEXEC_DIR)/mu-conference $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)/

install/runtime_scripts:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying runtime scripts"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)
	$(SILENT) $(CP) $(SRCROOT)/ichatserver_init_tool $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)/
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)/sbs_backup
	$(SILENT) $(CP) $(SRCROOT)/$(BACKUPRESTORE_SRC_DIR)/iChatServer_* $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)/sbs_backup/
	$(SILENT) $(MKDIR) -p -m 700 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/tmp
	
install/man_pages:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying additional man pages"
	$(SILENT) $(MKDIR) -p -m 755  $(OBJROOT)/$(STAGING_DIR)/$(MAN_DIR)/man8
	$(SILENT) $(CP) $(SRCROOT)/ichatserver_init_tool.8 $(OBJROOT)/$(STAGING_DIR)/$(MAN_DIR)/man8/
	$(SILENT) $(CP) $(SRCROOT)/mu-conference.8 $(OBJROOT)/$(STAGING_DIR)/$(MAN_DIR)/man8/
	$(SILENT) $(CP) $(SRCROOT)/$(AUTOBUDDY_SRC_DIR)/jabber_autobuddy.8 $(OBJROOT)/$(STAGING_DIR)/$(MAN_DIR)/man8/

install/custom_configs:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying Apple configuration files"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)
	$(SILENT) $(CP) -r $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/ppc/$(SYSCONFIG_DIR)/jabberd $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/
	$(SILENT) $(RM) -rf $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/ppc/$(SYSCONFIG_DIR)/jabberd
	$(SILENT) $(RM) -rf $(OBJROOT)/$(STAGING_DIR)/$(SYSCONFIG_DIR)
	$(SILENT) $(CP) $(SRCROOT)/cfg-apple/*.xml $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/
	$(SILENT) $(CP) $(SRCROOT)/cfg-apple/*.cfg $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/
	$(SILENT) $(CP) $(SRCROOT)/cfg-apple/muc-jcr.xml $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/
	$(SILENT) $(CHOWN) jabber:admin $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/*.xml
	$(SILENT) $(CHOWN) jabber:admin $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/jabberd.cfg
	$(SILENT) $(CHMOD) 660 $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/*.xml
	$(SILENT) $(CHMOD) 660 $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/jabberd.cfg

install/sbsbackup:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying SBS Backup files"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/sbs_backup
	$(SILENT) $(CP) $(SRCROOT)/$(BACKUPRESTORE_SRC_DIR)/75-iChatServer.plist $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/sbs_backup/

install/migration:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying migration tools"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/migration
	$(SILENT) $(MKDIR) -p -m 700 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/tmp
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(MIGRATION_EXTRAS_DIR)
	$(SILENT) $(CP) $(SRCROOT)/$(MIGRATION_SRC_DIR)/jabber_config_migrator.pl $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/migration/
	$(SILENT) $(CP) $(SRCROOT)/$(MIGRATION_SRC_DIR)/jabber_data_migrator.rb $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/migration/
	$(SILENT) $(CP) $(SRCROOT)/$(MIGRATION_SRC_DIR)/jabber_migration_selector.pl $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/migration/
	$(SILENT) $(CP) $(SRCROOT)/$(MIGRATION_SRC_DIR)/58_jabbermigrator.pl $(OBJROOT)/$(STAGING_DIR)/$(MIGRATION_EXTRAS_DIR)/

install/jabber_var_dirs:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying - creating Jabberd /var directories"
	$(SILENT) $(MKDIR) -p -m 750 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)
	$(SILENT) $(CP) $(OBJROOT)/$(JABBERD2_SRC_DIR)/ppc/tools/db-setup.sqlite $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/
	$(SILENT) $(MKDIR) -p -m 770 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/message_archives

install/file_proxy:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying File Proxy files"
	$(SILENT) $(MKDIR) -p -m 750 $(OBJROOT)/proxy65_install
	$(SILENT) $(MKDIR) -p -m 750 $(OBJROOT)/$(STAGING_DIR)/private/var/jabberd/log
	$(SILENT) $(CP) -R $(SRCROOT)/modules/filetransfer/proxy65 $(OBJROOT)/proxy65_install
	$(SILENT) $(CD) $(OBJROOT)/proxy65_install/proxy65 && python setup.py bdist_dumb
	$(SILENT) $(CD) $(OBJROOT)/proxy65_install/proxy65/dist/ && $(CP) Proxy*.tar.gz  $(DSTROOT)/Proxy65-1.0.0.tar.gz
	$(SILENT) $(GZIP) -d $(DSTROOT)/Proxy65-1.0.0.tar.gz
	$(SILENT) $(TAR) -xvf $(DSTROOT)/Proxy65-1.0.0.tar -C $(DSTROOT)/
	$(SILENT) $(RM) $(DSTROOT)/Proxy65-1.0.0.tar
	$(SILENT) $(MKDIR) -p -m 750 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/modules/proxy65
	$(SILENT) $(CP) $(SRCROOT)/modules/filetransfer/makeargtap $(OBJROOT)/$(STAGING_DIR)/private/var/jabberd/modules/proxy65/

install/copy_dstroot:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying staged content to DSTROOT"
	@echo "#"
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/private $(DSTROOT)/private
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/usr $(DSTROOT)/usr
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/System $(DSTROOT)/System
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...fixing DESTROOT ownership & permissions"
	$(SILENT) $(CHOWN) jabber:jabber $(DSTROOT)/private/var/jabberd
	$(SILENT) $(CHOWN) jabber:jabber $(DSTROOT)/private/var/jabberd/db-setup.sqlite
	$(SILENT) $(CHOWN) -R jabber:jabber $(DSTROOT)/private/var/jabberd/log
	$(SILENT) $(CHOWN) -R jabber:admin $(DSTROOT)/private/var/jabberd/message_archives
	$(SILENT) $(CHOWN) -R jabber:jabber $(DSTROOT)/private/var/jabberd/modules
	$(SILENT) $(CHOWN) -R jabber:jabber $(DSTROOT)/private/var/jabberd/tmp
	$(SILENT) $(CHOWN) root:wheel $(DSTROOT)/$(MAN_DIR)/man8/*

install/end_banner:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: BUILD COMPLETE."
	@echo "#"
	@echo "### The latest information about Jabber 2.0 is available on the web"
	@echo "### at http://jabberd.jabberstudio.org/2/.  Use Server Preferences"
	@echo "### or Server Admin app to set up and start iChat Server."

#
# MAIN INSTALL TARGET
#
# Executes all sub-targets and displays the completion banner
#
install: build \
		install/intro_banner install/jabber_usr_dir install/autobuddy \
		install/mu-conference install/runtime_scripts install/man_pages \
		install/custom_configs install/sbsbackup install/jabber_var_dirs \
		install/file_proxy install/migration install/copy_dstroot \
		install/end_banner

untar: libidn/untar jabberd2/untar

clean: libidn/clean jabberd2/clean

cleanobj: libidn/cleanobj jabberd2/cleanobj

clean-all: libidn/clean-all jabberd2/clean-all

installhdrs:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: No headers to install in $(SRCROOT)"
	@echo "#"

# installsrc target -- make sure that all source files to be installed 
#                      are included in the FILES_TO_INSTALL list
installsrc:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: Installing sources into $(SRCROOT)"
	@echo "#"
	$(SILENT) -$(RM) -rf $(SRCROOT)
	$(SILENT) $(MKDIRS) $(SRCROOT)
	$(SILENT) $(CP) -r $(FILES_TO_INSTALL) $(SRCROOT)
#	$(SILENT) install-src-run=1

#
# Rules for special build directories
#
#
$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@

$(OBJROOT):
	$(SILENT) $(MKDIRS) $@
