##
# Apple wrapper Makefile for cyrus imap server
# Copyright (c) 2003 by Apple Computer, Inc.
##
# Although it is a GNU-like project, it does not come with a Makefile,
# and the configure script requires user interaction. This Makefile just provides
# the targets required by Apple's build system, and creates a config file
# config.php by modifying the default config file config_default.php
#
# This Makefile tries to conform to hier(7) by moving config, data, attachments
# to appropriate places in the file system, and makes symlinks where necessary.

PROJECT_NAME=cyrus_imap
PROJECT_VERSION=2.1.13
PROJECT_DIR=$(PROJECT_NAME)-$(PROJECT_VERSION)
PROJECT_ARCHIVE=$(PROJECT_DIR).tar.gz

PATCH_NAME=patch
PATCH_VERSION=1.1
PATCH_DIR=$(PATCH_NAME)-$(PATCH_VERSION)
PATCH_ARCHIVE=$(PATCH_DIR).tar.gz

EXTRAS_NAME=extras
EXTRAS_VERSION=1.0
EXTRAS_DIR=$(EXTRAS_NAME)-$(EXTRAS_VERSION)
EXTRAS_ARCHIVE=$(EXTRAS_DIR).tar.gz

EXPORT_TOOL_NAME=amsmailtool
EXPORT_TOOL_VERSION=1.0
EXPORT_TOOL_DIR=$(EXPORT_TOOL_NAME)-$(EXPORT_TOOL_VERSION)
EXPORT_TOOL_ARCHIVE=$(EXPORT_TOOL_DIR).tar.gz

DLLIB_NAME=dlcompat
DLLIB_VERSION=20010505
DLLIB_DIR=$(DLLIB_NAME)-$(DLLIB_VERSION)
DLLIB_ARCHIVE=$(DLLIB_DIR).tar.gz

SILENT=@
ECHO=echo
CP=cp
RM=rm
MV=mv
RANLIB=ranlib
DSTROOT=/
ETCDIR=/private/etc
SHAREDIR=/usr/share/man
TOOLSDIR=/usr/bin/cyrus/tools
SASCRIPTS= cyrus
SASCRIPTSDIR=/System/Library/ServerSetup/SetupExtras
SETUPEXTRASDIR=SetupExtras
GNUTAR=gnutar
PROJECT_FILES=Makefile $(PATCH_ARCHIVE) $(DB_ARCHIVE) $(EXTRAS_ARCHIVE) $(DLLIB_ARCHIVE) $(EXPORT_TOOL_ARCHIVE)
CYRUS_TOOLS= dohash mkimap mupdate-loadgen.pl not-mkdep rehash \
			 translatesieve undohash upgradesieve
				
# Configuration values we customize
#

CYRUS_CONFIG = \
	--host=powerpc-apple \
	--with-dbdir=/usr/local/BerkeleyDB \
	--with-sasl=/usr \
	--with-openssl=/usr \
	--with-auth=krb \
	--enable-murder \
	--with-pidfile=/var/run/cyrus-master.pid \
	--with-cyrus-prefix=$(DSTROOT)/usr/bin/cyrus \
	--prefix=$(DSTROOT)/usr/bin/cyrus \
	--exec-prefix=$(DSTROOT)/usr/bin/cyrus

# These includes provide the proper paths to system utilities
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

# Set up our variables
#

GNUTAR=gnutar

# Build rules

default:: do_untar configure do_build

default:: do_untar configure do_build

install:: configure do_build do_install do_clean

installlocal:: configure do_build do_install

clean:: do_clean

configure:: do_untar do_build_db do_patch do_configure

installhdrs:: do_installhdrs

installsrc:: do_installsrc

# Custom configuration:
#

do_untar:
	$(SILENT) $(ECHO) "Untarring $(PROJECT_DIR)..."
	$(SILENT) if [ ! -e $(PROJECT_DIR)/README ]; then\
		$(GNUTAR) -xzf $(PROJECT_ARCHIVE);\
	fi
	$(SILENT) $(ECHO) "Untarring $(PATCH_DIR)..."
	$(SILENT) if [ ! -e $(PATCH_DIR)/README.PATCH ]; then\
		$(GNUTAR) -xzf $(PATCH_ARCHIVE);\
	fi
	$(SILENT) $(ECHO) "Untarring $(EXTRAS_DIR)..."
	$(SILENT) if [ ! -e $(EXTRAS_DIR)/README ]; then\
		$(GNUTAR) -xzf $(EXTRAS_ARCHIVE);\
	fi
	$(SILENT) $(ECHO) "Untarring $(DLLIB_DIR)..."
	$(SILENT) if [ ! -e $(DLLIB_DIR)/README ]; then\
		$(GNUTAR) -xzf $(DLLIB_ARCHIVE);\
	fi
	$(SILENT) $(ECHO) "Untarring $(EXPORT_TOOL_DIR)..."
	$(SILENT) if [ ! -e $(EXPORT_TOOL_DIR)/README ]; then\
		$(GNUTAR) -xzf $(EXPORT_TOOL_ARCHIVE);\
	fi
	$(SILENT) $(ECHO) "Untarring complete."

do_patch:
	$(SILENT) $(ECHO) "Applying $(PATCH_DIR) to source $(PROJECT_NAME)..."
	$(SILENT) ($(CD) "$(SRCROOT)/$(PATCH_DIR)" && $(CP) -r * "$(SRCROOT)/$(PROJECT_DIR)")
	$(SILENT) $(ECHO) "Applying $(PATCH_NAME) patch complete."

do_configure: 
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
#	$(SILENT) ($(CD) "$(SRCROOT)/$(EXTRAS_DIR)/lib" && $(CP) -r /usr/lib/sasl2 .)
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_DIR)" && ./configure $(CYRUS_CONFIG))
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME) complete."

do_install: $(DSTROOT) $(DSTROOT)$(ETCDIR) $(DSTROOT)$(TOOLSDIR) $(DSTROOT)$(SHAREDIR) $(DSTROOT)$(SASCRIPTSDIR)
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME)..."
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_DIR)" && make install)
	for file in $(CYRUS_TOOLS); \
	do \
		$(CD) "$(SRCROOT)/$(PROJECT_DIR)/tools" && $(CP) $$file $(DSTROOT)$(TOOLSDIR) || exit 1; \
	done
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_DIR)/etc" && $(CP) cyrus.conf.default $(DSTROOT)$(ETCDIR))
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_DIR)/etc" && $(CP) imapd.conf.default $(DSTROOT)$(ETCDIR))
	$(SILENT) ($(MV) "$(DSTROOT)/usr/bin/cyrus/man/man8/master.8" "$(DSTROOT)/usr/bin/cyrus/man/man8/cyrus-master.8" )
	$(SILENT) ($(CD) "$(DSTROOT)/usr/bin/cyrus/man" && $(CP) -r * "$(DSTROOT)/$(SHAREDIR)/" )
	$(SILENT) ($(RM) -r "$(DSTROOT)/usr/bin/cyrus/include" )
	$(SILENT) ($(RM) -r "$(DSTROOT)/usr/bin/cyrus/man" )
	$(SILENT) ($(RM) -r "$(DSTROOT)/usr/bin/cyrus/lib" )
	$(SILENT) if [ -e "$(DSTROOT)/usr/bin/cyrus/Library" ]; then\
		$(RM) -r "$(DSTROOT)/usr/bin/cyrus/Library";\
	fi
	$(SILENT) if [ -e "$(DSTROOT)/usr/bin/cyrus/System" ]; then\
		$(RM) -r "$(DSTROOT)/usr/bin/cyrus/System";\
	fi
	$(SILENT) if [ -e "$(DSTROOT)/usr/bin/cyrususr" ]; then\
		$(MV) "$(DSTROOT)/usr/bin/cyrususr" "$(DSTROOT)/usr/bin/cyrus/";\
	fi
	$(SILENT) ($(CD) "$(SRCROOT)/$(EXPORT_TOOL_DIR)" && /usr/bin/pbxbuild install DSTROOT="$(DSTROOT)")
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_DIR)/$(SETUPEXTRASDIR)" && $(CP) cyrus $(DSTROOT)$(SASCRIPTSDIR))
	for file in $(SASCRIPTS); \
	do \
		$(CD) "$(SRCROOT)/$(PROJECT_DIR)/$(SETUPEXTRASDIR)" && $(CP) $$file $(DSTROOT)$(SASCRIPTSDIR) || exit 1; \
	done
	$(SILENT) $(ECHO) "Install $(PROJECT_NAME) complete."

do_build_db:
	$(SILENT) $(ECHO) "Building $(DLLIB_DIR)...."
	$(SILENT) ($(CD) "$(SRCROOT)/$(DLLIB_DIR)" && make install prefix="$(SRCROOT)/$(EXTRAS_DIR)")
	$(SILENT) ($(CD) "$(SRCROOT)/$(EXTRAS_DIR)/lib" && $(RANLIB) libdl.a)
	$(SILENT) ($(CD) "$(SRCROOT)/$(EXTRAS_DIR)/lib" && $(RM) libdl.dylib)
	$(SILENT) $(ECHO) "Building $(DLLIB_DIR) Complete."

do_build: $(DSTROOT) $(DSTROOT)$(ETCDIR) $(DSTROOT)$(TOOLSDIR) $(DSTROOT)$(SHAREDIR)
	$(SILENT) $(ECHO) "Building $(PROJECT_NAME)...."
	$(SILENT) $(ECHO) "Building $(PROJECT_DIR)...."
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_DIR)" && make)
	$(SILENT) $(ECHO) "Building $(PROJECT_DIR) Complete."
	$(SILENT) $(ECHO) "Building $(EXPORT_TOOL_DIR)...."
	$(SILENT) ($(CD) "$(SRCROOT)/$(EXPORT_TOOL_DIR)" && /usr/bin/pbxbuild DSTROOT="$(DSTROOT)")
	$(SILENT) $(ECHO) "Building $(EXPORT_TOOL_DIR) Complete."
	$(SILENT) $(ECHO) "Build complete."

do_installhdrs:
	$(SILENT) $(ECHO) "No headers to install"

do_installsrc:
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME) sources in $(SRCROOT)..."
	$(SILENT) -$(RM) -rf $(SRCROOT)
	$(SILENT) $(MKDIRS) $(SRCROOT)
	$(SILENT) $(CP) $(PROJECT_FILES) $(SRCROOT)
	$(SILENT) $(CP) $(PROJECT_ARCHIVE) $(SRCROOT)

do_clean:
	$(SILENT) $(ECHO) "Cleaning $(PROJECT_NAME)..."
	$(SILENT) -$(RM) -rf $(PROJECT_DIR)
	$(SILENT) -$(RM) -rf $(PATCH_DIR)
	$(SILENT) -$(RM) -rf $(EXTRAS_DIR)
	$(SILENT) -$(RM) -rf $(DLLIB_DIR)
	$(SILENT) -$(RM) -rf $(EXPORT_TOOL_DIR)
	$(SILENT) -$(RM) -rf $(DSTROOT)/usr/bin/cyrus/Prefix/lib/perl5
	$(SILENT) $(ECHO) "Cleaning complete."

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(ETCDIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(TOOLSDIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SHAREDIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SASCRIPTSDIR):
	$(SILENT) $(MKDIRS) $@
