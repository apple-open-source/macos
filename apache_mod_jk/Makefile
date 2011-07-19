#
# Copyright (c) 2003-2010 Apple Inc.
# Configure and build Apache mod_jk, the connector to Tomcat
# Also installs workers.properties
#

# These includes provide the proper paths to system utilities

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

CONNECTORS_VERS=1.2.30
CONNECTORS=tomcat-connectors-$(CONNECTORS_VERS)-src
CONNECTORS_APACHE_DIR2=$(CONNECTORS)/native/apache-2.0
CONNECTORS_JKNATIVE_DIR=$(CONNECTORS)/native
VERSIONS_DIR=/usr/local/OpenSourceVersions
LICENSE_DIR=/usr/local/OpenSourceLicenses
MOD_JK=mod_jk.so
APXS2=/usr/sbin/apxs

INSTALL=/usr/bin/install
WORKERS=workers.properties
LIBEXECDIR2=$(shell $(APXS2) -q LIBEXECDIR)
SYSCONFDIR2=$(shell $(APXS2) -q SYSCONFDIR)

build all default:: configure2 build2

clean:
	$(SILENT) $(ECHO) "Cleaning..."
	$(SILENT) -$(RM) -rf *.o
	$(SILENT) -$(RM) -rf $(CONNECTORS)

installhdrs:
	$(SILENT) $(ECHO) "$(PROJECT_NAME) has no headers to install in $(SRCROOT)..."

installsrc:
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME) sources in $(SRCROOT)..."
	$(SILENT) -$(RM) -rf $(SRCROOT)
	$(SILENT) $(MKDIRS) $(SRCROOT)
	$(SILENT) $(CP) $(CONNECTORS).tar.gz $(WORKERS) Makefile apache_mod_jk.plist apache_mod_jk.txt $(SRCROOT)

$(CONNECTORS_JKNATIVE_DIR)/configure: $(OBJROOT)
	$(SILENT) -$(RM) -rf $(CONNECTORS) 
	$(SILENT) $(TAR) -xzf $(CONNECTORS).tar.gz
	$(SILENT) $(ECHO) "Patching configure script..."
	$(SILENT) $(CP) $(CONNECTORS_JKNATIVE_DIR)/configure $(CONNECTORS_JKNATIVE_DIR)/configure.orig
	$(SILENT) $(SED) < $(CONNECTORS_JKNATIVE_DIR)/configure.orig > $(CONNECTORS_JKNATIVE_DIR)/configure \
		-e 's%sleep 1%/bin/sleep 1%'

#$(CONNECTORS_JKNATIVE_DIR)/configure: $(CONNECTORS)
#	cd $(CONNECTORS_JKNATIVE_DIR); ./buildconf.sh

configure2: $(CONNECTORS_JKNATIVE_DIR)/configure
	$(SILENT) $(CD) $(CONNECTORS_JKNATIVE_DIR);\
	CXX=gcc \
	LDFLAGS="$$RC_CFLAGS" \
	CFLAGS="$$RC_CFLAGS" \
	./configure --with-apxs=$(APXS2) --enable-prefork

build2:
	$(SILENT) $(ECHO) "Building Apache 2.2 module..."
	$(SILENT) $(CD) $(CONNECTORS_APACHE_DIR2);make -f Makefile.apxs

install2: $(CONNECTORS_APACHE_DIR2)/.libs/$(MOD_JK) $(DSTROOT)$(LIBEXECDIR2) $(DSTROOT)$(SYSCONFDIR2) $(DSTROOT)$(VERSIONS_DIR) $(DSTROOT)$(LICENSE_DIR)
	$(SILENT) $(ECHO) "Installing Apache 2.2 module..."
	$(SILENT) $(CD) $(CONNECTORS_APACHE_DIR2)/.libs;cp $(MOD_JK) $(DSTROOT)$(LIBEXECDIR2)
	chmod 644 $(DSTROOT)$(LIBEXECDIR2)/$(MOD_JK)
	$(STRIP) -S $(DSTROOT)$(LIBEXECDIR2)/$(MOD_JK)
	$(SILENT) $(INSTALL) -m 644 $(WORKERS) $(DSTROOT)$(SYSCONFDIR2)
	$(SILENT) $(INSTALL) -m 444 $(WORKERS) $(DSTROOT)$(SYSCONFDIR2)/$(WORKERS).default
	$(INSTALL) -m 444 -o root -g wheel apache_mod_jk.plist $(DSTROOT)$(VERSIONS_DIR)
	$(INSTALL) -m 444 -o root -g wheel apache_mod_jk.txt $(DSTROOT)$(LICENSE_DIR)


install: configure2 build2 install2

$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(LIBEXECDIR2):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SYSCONFDIR2):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(VERSIONS_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(LICENSE_DIR):
	$(SILENT) $(MKDIRS) $@

$(OBJROOT):
	$(SILENT) $(MKDIRS) $@

