#
# Apple wrapper Makefile for PHP
# Copyright (c) 2008-2012 Apple Inc. All Rights Reserved.
##
#

# General project info for use with RC/GNUsource.make makefile
Project         = php
ProjectName     = apache_mod_php
UserType        = Developer
ToolType        = Commands
Submission      = 74

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
			--with-apxs2=$(USRSBINDIR)/apxs \
			--enable-cli \
			--with-config-file-path=/etc \
			--with-libxml-dir=$(USRDIR) \
			--with-openssl=$(USRDIR) \
			--with-kerberos=$(USRDIR) \
			--with-zlib=$(USRDIR) \
			--enable-bcmath \
			--with-bz2=$(USRDIR) \
			--enable-calendar \
			--disable-cgi \
			--with-curl=$(USRDIR) \
			--enable-dba \
				--enable-ndbm=$(USRDIR) \
			--enable-exif \
			--enable-fpm \
			--enable-ftp \
			--with-gd \
				--with-freetype-dir=$(DSTROOT)$(USRDIR)/local \
				--with-jpeg-dir=$(DSTROOT)$(USRDIR)/local \
				--with-png-dir=$(DSTROOT)$(USRDIR)/local \
				--enable-gd-native-ttf \
			--with-icu-dir=$(USRDIR) \
			--with-iodbc=$(USRDIR) \
			--with-ldap=$(USRDIR) \
				--with-ldap-sasl=$(USRDIR) \
			--with-libedit=$(USRDIR) \
			--enable-mbstring \
			--enable-mbregex \
			--with-mysql=mysqlnd \
			--with-mysqli=mysqlnd \
			--without-pear \
			--with-pdo-mysql=mysqlnd \
				--with-mysql-sock=/var/mysql/mysql.sock \
			--with-readline=$(USRDIR) \
			--enable-shmop \
			--with-snmp=$(USRDIR) \
			--enable-soap \
			--enable-sockets \
			--enable-sqlite-utf8 \
			--enable-suhosin \
			--enable-sysvmsg --enable-sysvsem --enable-sysvshm \
			--with-tidy \
			--enable-wddx \
			--with-xmlrpc \
				--with-iconv-dir=$(USRDIR) \
			--with-xsl=$(USRDIR) \
			--enable-zend-multibyte \
			--enable-zip


# Additional project info used with AEP
AEP		= YES
AEP_Version	= 5.3.15
AEP_LicenseFile	= $(Sources)/LICENSE
AEP_Patches	= suhosin-patch-5.3.9-0.9.10.patch \
			MacOSX_build.patch arches.patch \
			iconv.patch mysql_sock.patch pear.patch phar.patch \
			xdebug.patch fpm.patch dSYM.patch
AEP_ConfigDir	= $(ETCDIR)
AEP_Binaries	= $(shell $(USRSBINDIR)/apxs -q LIBEXECDIR)/*.so $(USRBINDIR)/php $(USRSBINDIR)/php-fpm
AEP_ManPages	= pear.1 phar.1 phar.phar.1
Dependencies	= freetype libjpeg libpng
GnuAfterInstall	= archive-strip-binaries install-macosx install-xdebug


# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: post-extract-source $(GnuAfterInstall)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make

# Override settings from above includes
BuildDirectory	= $(OBJROOT)/Build/$(AEP_Project)
Install_Target	= install
TMPDIR		= $(OBJROOT)/Build/tmp
# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"

#
# "ifeq / ifneq" are processed at read time; since these use variables defined
# by the included files above, this must be performed after the includes.
#
# The PCRE library is only installed on Snow Leopard and later.
ifneq ($(strip $(wildcard $(USRDIR)/local/include/pcre.*)),)
Extra_Configure_Flags	+= --with-pcre-regex=$(USRDIR)
endif
# The PostgreSQL library is only installed on Lion and later.
ifneq ($(strip $(wildcard $(USRINCLUDEDIR)/postgres*)),)
Extra_Configure_Flags	+= --with-pgsql=$(USRDIR) --with-pdo-pgsql=$(USRDIR)
endif


# Build rules
# Dependency info necessary to ensure temp directory and cleanup are performed.
lazy_install_source:: $(TMPDIR)
$(GNUConfigStamp): post-extract-source $(TMPDIR)

# Post-extract target
post-extract-source: extract-source
	@echo "Executing extra patch after extraction..."
	$(PERL) -i -pe 's|-i -a -n php5|-i -n php5|g' $(Sources)/configure
	$(PERL) -i -pe 's|rm -f conftest|rm -rf conftest|g' $(Sources)/configure

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
	$(PERL) -i -pe 's|^extension_dir =.*|extension_dir = $(USRLIBDIR)/php/extensions/no-debug-non-zts-20060613|' $(DSTROOT)$(AEP_ConfigDir)/php.ini.default
	$(INSTALL_DIRECTORY) $(DSTROOT)$(USRLIBDIR)/php/extensions/no-debug-non-zts-20060613
	@echo "Removing references to DSTROOT in php-config and include files..."
	$(CP) $(DSTROOT)$(USRBINDIR)/php-config $(SYMROOT)/php-config \
		&& $(SED) -e 's=-L$(DSTROOT)$(USRDIR)/local/lib==' $(SYMROOT)/php-config \
		| $(SED) -e 's@$(DSTROOT)@@g' > $(DSTROOT)$(USRBINDIR)/php-config
	$(CP) $(DSTROOT)$(USRINCLUDEDIR)/$(Project)/main/build-defs.h $(SYMROOT) \
		&& LANG=C $(SED) -e 's@$(DSTROOT)@@g' $(SYMROOT)/build-defs.h \
			> $(DSTROOT)$(USRINCLUDEDIR)/$(Project)/main/build-defs.h
	@echo "Archiving private static libraries..."
	-$(MV) $(DSTROOT)$(USRDIR)/local/lib/* $(SYMROOT)
	@echo "Deleting private dependencies..."
	-$(RMDIR) $(DSTROOT)$(USRDIR)/local/lib
	-$(RMDIR) $(DSTROOT)$(USRDIR)/local/include
	@echo "Installing PEAR phar for installation at setup time."
	$(INSTALL_FILE) $(SRCROOT)/install-pear-nozlib.phar $(DSTROOT)$(USRLIBDIR)/php
	@echo "Fixing PEAR configuration file..."
	if [ -e $(DSTROOT)/$(USRLIBDIR)/php/pearcmd.php ]; then	\
		$(CP) $(DSTROOT)/$(USRLIBDIR)/php/pearcmd.php $(PEAR_Cmd);	\
		$(PATCH) -l $(PEAR_Cmd) $(SRCROOT)/patches/pearcmd.patch;	\
		$(PEAR) -C $(DSTROOT)$(ETCDIR)/pear.conf config-set \
			cache_dir /tmp/pear/cache system;	\
		$(PEAR) -C $(DSTROOT)$(ETCDIR)/pear.conf config-set \
			download_dir /tmp/pear/download system;	\
		$(PEAR) -C $(DSTROOT)$(ETCDIR)/pear.conf config-set \
			temp_dir /tmp/pear/temp system;		\
	fi
	@echo "Cleaning up PEAR junk files..."
	-$(RMDIR) $(DSTROOT)$(USRLIBDIR)/php/test
	-$(RM) -rf $(DSTROOT)/.channels \
		$(DSTROOT)/.depdb \
		$(DSTROOT)/.depdblock \
		$(DSTROOT)/.filemap \
		$(DSTROOT)/.lock \
		$(DSTROOT)/.registry \
		$(DSTROOT)$(USRLIBDIR)/php/.lock \
		$(DSTROOT)$(USRLIBDIR)/php/.depdblock
	-$(STRIP) -x $(DSTROOT)/usr/bin/php $(DSTROOT)/usr/sbin/php-fpm
	-$(RM) -rf $(DSTROOT)/usr/var
	@echo "Mac OS X-specific cleanup complete."

install-xdebug:
	@echo "Installing XDebug extension..."
	$(MAKE) -C xdebug $(TARGET)				\
			SRCROOT=$(SRCROOT)/xdebug		\
			OBJROOT=$(OBJROOT)			\
			SYMROOT=$(SYMROOT)			\
			DSTROOT=$(DSTROOT)			\
			BuildDirectory=$(OBJROOT)/Build/xdebug	\
			Sources=$(OBJROOT)/xdebug		\
			CoreOSMakefiles=$(CoreOSMakefiles)
	@echo "XDebug extension installed."

$(DSTROOT)$(LIBEXECDIR)/apache2 $(TMPDIR):
	$(MKDIR) $@

refresh-pear:
	curl -RO# http://pear.php.net/install-pear-nozlib.phar
	curl -RO# http://pear2.php.net/pyrus.phar
