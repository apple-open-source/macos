#
# Apple wrapper Makefile for PHP/FPM
# Copyright (c) 2008-2013 Apple Inc. All Rights Reserved.
##
#

# General project info for use with RC/GNUsource.make makefile
Project         = php
ProjectName     = phpfpm
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
AEP_Version	= 5.3.26
AEP_LicenseFile	= $(Sources)/LICENSE
AEP_Patches	= suhosin-patch-5.3.23-0.9.10.patch \
			MacOSX_build.patch arches.patch \
			iconv.patch mysql_sock.patch phar.patch \
			dSYM.patch 
AEP_ConfigDir	= $(ETCDIR)
AEP_Binaries	= $(USRSBINDIR)/php-fpm
AEP_ManPages	= 
GnuAfterInstall        = archive-strip-binaries install-macosx

# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: post-extract-source $(GnuAfterInstall)

install::

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make
# Override settings from above includes
BuildDirectory	= $(OBJROOT)/Build/$(AEP_Project)
Install_Target	= install
Install_Prefix  = $(USRDIR)
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

install-macosx:
	@echo "Cleaning up php-fpm install for Mac OS X..."
	-$(RMDIR) $(DSTROOT)$(ETCDIR)/apache2
	$(CHOWN) -R root:wheel $(DSTROOT)/
	$(INSTALL_FILE) $(Sources)/php.ini-production $(DSTROOT)$(AEP_ConfigDir)/php.ini.default
	$(PERL) -i -pe 's|^extension_dir =.*|extension_dir = $(USRLIBDIR)/php/extensions/no-debug-non-zts-20060613|' $(DSTROOT)$(AEP_ConfigDir)/php.ini.default
	$(INSTALL_DIRECTORY) $(DSTROOT)$(USRLIBDIR)/php/extensions/no-debug-non-zts-20060613
	@echo "Deleting private dependencies..."
	@echo "Installing PEAR phar for installation at setup time."
	-$(RMDIR) $(DSTROOT)$(USRLIBDIR)/php/test
	$(MKDIR) $(DSTROOT)/usr/sbin
	-$(STRIP) -x $(BuildDirectory)/sapi/fpm/php-fpm -o $(DSTROOT)/usr/sbin/php-fpm
	$(MKDIR) $(DSTROOT)/usr/share/man/man8
	-$(CP) $(BuildDirectory)/sapi/fpm/php-fpm.8 $(DSTROOT)/usr/share/man/man8
	-$(RM) -rf $(DSTROOT)/usr/var
	@echo "Deleting private dependencies..."
	-$(RMDIR) $(DSTROOT)$(USRINCLUDEDIR)
	-$(RMDIR) $(DSTROOT)$(USRLIBDIR)
	-$(RMDIR) $(DSTROOT)/sbin
	@echo "Mac OS X-specific cleanup complete."

$(TMPDIR):
	$(MKDIR) $@

