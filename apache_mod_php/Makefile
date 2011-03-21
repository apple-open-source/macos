#
# Apple wrapper Makefile for PHP
# Copyright (c) 2008-2010 Apple Inc. All Rights Reserved.
##
#

# General project info for use with RC/GNUsource.make makefile
Project         = php
ProjectName     = apache_mod_php
UserType        = Developer
ToolType        = Commands
Submission      = 53.4

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Environment	= YACC=/usr/local/bin/bison-1.28 \
			php_cv_bison_version=1.28 \
			LEX=/usr/local/bin/lex-2.5.4 \
			MAKEOBJDIR="$(BuildDirectory)" \
			INSTALL_ROOT="$(DSTROOT)" \
			TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)"
# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment	= CFLAGS="$$RC_CFLAGS -Os -g" \
					LDFLAGS="$$RC_CFLAGS -Os -g" \
					EXTRA_LIBS="-lresolv" \
					EXTRA_LDFLAGS_PROGRAM="-mdynamic-no-pic"

# The configure flags are ordered to match current output of ./configure --help.
# Extra indentation represents suboptions.
Extra_Configure_Flags	= --sysconfdir=$(ETCDIR) \
			--with-apxs2=/usr/sbin/apxs \
			--enable-cli \
			--with-config-file-path=/etc \
			--with-libxml-dir=/usr \
			--with-openssl=/usr \
			--with-kerberos=/usr \
			--with-zlib=/usr \
			--enable-bcmath \
			--with-bz2=/usr \
			--enable-calendar \
			--with-curl=/usr \
			--enable-exif \
			--enable-ftp \
			--with-gd \
				--with-jpeg-dir=$(DSTROOT)/usr/local \
				--with-png-dir=$(DSTROOT)/usr/local \
				--enable-gd-native-ttf \
			--with-ldap=/usr \
				--with-ldap-sasl=/usr \
			--enable-mbstring \
			--enable-mbregex \
			--with-mysql=mysqlnd \
			--with-mysqli=mysqlnd \
			--with-pdo-mysql=mysqlnd \
				--with-mysql-sock=/var/mysql/mysql.sock \
			--with-iodbc=/usr \
			--enable-shmop \
			--with-snmp=/usr \
			--enable-soap \
			--enable-sockets \
			--enable-sysvmsg --enable-sysvsem --enable-sysvshm \
			--enable-wddx \
			--with-xmlrpc \
				--with-iconv-dir=/usr \
			--with-xsl=/usr \
			--enable-zend-multibyte \
			--enable-zip
# MySQL is only installed on X Server.
#ifneq ($(strip $(wildcard /usr/bin/mysql_conf*)),)
#Extra_Configure_Flags	+= --with-mysql=/usr \
#				--with-mysql-sock=/var/mysql \
#				--with-mysqli=/usr/bin/mysql_config \
#				--with-pdo-mysql=/usr/bin/mysql_config
#endif
# The PCRE library is only installed on Snow Leopard.
ifneq ($(strip $(wildcard /usr/local/include/pcre.*)),)
Extra_Configure_Flags	+= --with-pcre-regex=/usr
endif

# Let this Makefile manage source copying.
CommonNoInstallSource	= YES

# Additional project info used with AEP
AEP		= YES
AEP_Version	= 5.3.4
AEP_LicenseFile	= $(Sources)/LICENSE
AEP_Patches	= MacOSX_build.patch arches.patch \
			iconv.patch mysql_sock.patch pear.patch phar.patch
AEP_ConfigDir	= $(ETCDIR)
AEP_ManPages	= pear.1 phar.1 phar.phar.1
Dependencies	= libjpeg libpng
GnuAfterInstall	= install-macosx

# Used only in this file
PROJECT_FILES	= Makefile AEP.make $(ProjectName).plist $(AEP_ManPages)

# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: do_installsrc post-extract-source build-dependencies $(GnuAfterInstall)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include AEP.make

# Override settings from above includes
BuildDirectory	= $(OBJROOT)/Build/$(AEP_Project)
Install_Target	= install
TMPDIR		= $(OBJROOT)/Build/tmp
# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"


# Build rules
$(GNUConfigStamp): post-extract-source build-dependencies

# With Common.make, use install_source instead of installsrc.
install_source:: do_installsrc

do_installsrc:
	@echo "Installing $(ProjectName) sources in $(SRCROOT)..."
	-$(RMDIR) $(SRCROOT)
	$(MKDIR) $(SRCROOT)
	$(CP) $(PROJECT_FILES) $(SRCROOT)
	$(CP) $(AEP_Filename) $(SRCROOT)
ifdef Dependencies
	$(CP) $(Dependencies) $(SRCROOT)
endif
ifdef SubProjects
	$(CP) $(SubProjects) $(SRCROOT)
endif
ifdef AEP_Patches
	$(MKDIR) $(SRCROOT)/patches
	for patchfile in $(AEP_Patches); do \
		$(CP) patches/$$patchfile $(SRCROOT)/patches; \
	done
endif
	$(CP) patches/pearcmd.patch $(SRCROOT)/patches

# Common.make's recurse doesn't reset SRCROOT and misdefines Sources
build-dependencies: $(TMPDIR)
	$(_v) for Dependency in $(Dependencies); do			\
		$(MAKE) -C $${Dependency} $(TARGET)			\
			SRCROOT=$(SRCROOT)/$${Dependency}		\
			OBJROOT=$(OBJROOT)				\
			SYMROOT=$(SYMROOT)				\
			DSTROOT=$(DSTROOT)				\
			BuildDirectory=$(OBJROOT)/Build/$${Dependency}	\
			Sources=$(OBJROOT)/$${Dependency}		\
			CoreOSMakefiles=$(CoreOSMakefiles);		\
		done

# Post-extract target
post-extract-source: extract-source
	@echo "Executing extra patch after extraction..."
	$(PERL) -i -pe 's|-i -a -n php5|-i -n php5|g' $(Sources)/configure

# Invoke pearcmd.php manually (instead of via /usr/bin/pear) so we can force
# lookups from DSTROOT instead of final install location.
PEAR		= $(DSTROOT)$(USRBINDIR)/php -C -q \
		-n -d include_path=$(DSTROOT)$(USRLIBDIR)/php $(PEAR_Cmd)
PEAR_Cmd	= $(TMPDIR)/pearcmd.php

install-macosx:
	@echo "Cleaning up install for Mac OS X..."
	-$(RMDIR) $(DSTROOT)$(ETCDIR)/apache2
	$(CHOWN) -R root:wheel $(DSTROOT)/
	$(INSTALL_FILE) $(Sources)/php.ini-production $(DSTROOT)$(AEP_ConfigDir)/php.ini.default
	$(PERL) -i -pe 's|^extension_dir =.*|extension_dir = /usr/lib/php/extensions/no-debug-non-zts-20060613|' $(DSTROOT)$(AEP_ConfigDir)/php.ini.default
	@echo "$(INSTALL_DIRECTORY) $(DSTROOT)/usr/lib/php/extensions/no-debug-non-zts-20060613"
	@echo "Removing references to DSTROOT in php-config and include files..."
	$(CP) $(DSTROOT)$(USRBINDIR)/php-config $(SYMROOT)/php-config \
		&& $(SED) -e 's=-L$(DSTROOT)$(USRDIR)/local/lib==' $(SYMROOT)/php-config \
		| $(SED) -e 's@$(DSTROOT)@@g' > $(DSTROOT)$(USRBINDIR)/php-config
	$(CP) $(DSTROOT)$(USRINCLUDEDIR)/$(Project)/main/build-defs.h $(SYMROOT) \
		&& $(SED) -e 's@$(DSTROOT)@@g' $(SYMROOT)/build-defs.h \
			> $(DSTROOT)$(USRINCLUDEDIR)/$(Project)/main/build-defs.h
	@echo "Archiving and stripping binaries..."
	if [ ! -d $(SYMROOT) ]; then \
		$(MKDIR) -m 755 $(SYMROOT); \
	fi
	$(_v) for file in "$(DSTROOT)`/usr/sbin/apxs -q LIBEXECDIR`/"*.so $(DSTROOT)/usr/bin/php;	\
	do \
		$(CP) $${file} $(SYMROOT);	\
		$(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file};	\
		$(STRIP) -S $${file};		\
	done
	-$(MV) $(DSTROOT)$(USRDIR)/local/lib/* $(SYMROOT)
	@echo "Deleting private dependencies..."
	-$(RMDIR) $(DSTROOT)$(USRDIR)/local/lib
	-$(RMDIR) $(DSTROOT)$(USRDIR)/local/include
	@echo "Fixing PEAR configuration file..."
	if [ -e $(DSTROOT)/$(USRLIBDIR)/php/pearcmd.php ]; then \
		$(CP) $(DSTROOT)/$(USRLIBDIR)/php/pearcmd.php $(PEAR_Cmd);	\
		$(PATCH) -l $(PEAR_Cmd) $(SRCROOT)/patches/pearcmd.patch;	\
		$(PEAR) -C $(DSTROOT)$(ETCDIR)/pear.conf config-set \
			cache_dir /tmp/pear/cache system;	\
		$(PEAR) -C $(DSTROOT)$(ETCDIR)/pear.conf config-set \
			download_dir /tmp/pear/download system;	\
		$(PEAR) -C $(DSTROOT)$(ETCDIR)/pear.conf config-set \
			temp_dir /tmp/pear/temp system;	\
	fi
	@echo "Cleaning up PEAR junk files..."
	-$(RMDIR) $(DSTROOT)/usr/lib/php/test
	-$(RM) -rf $(DSTROOT)/.channels \
		$(DSTROOT)/.depdb \
		$(DSTROOT)/.depdblock \
		$(DSTROOT)/.filemap \
		$(DSTROOT)/.lock \
		$(DSTROOT)/.registry \
		$(DSTROOT)/usr/lib/php/.lock \
		$(DSTROOT)/usr/lib/php/.depdblock \
	@echo "Mac OS X-specific cleanup complete."

$(DSTROOT) $(DSTROOT)$(ETCDIR) $(DSTROOT)/usr/libexec/apache2 $(TMPDIR):
	$(MKDIR) $@
