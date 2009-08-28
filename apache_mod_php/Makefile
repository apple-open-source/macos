#
# Apple wrapper Makefile for PHP
# Copyright (c) 2008-2009 Apple Inc. All Rights Reserved.
##
#

# General project info for use with RC/GNUsource.make makefile
Project         = php
ProjectName     = apache_mod_php
UserType        = Developer
ToolType        = Plugin
Submission      = 53

GnuAfterInstall	= install-macosx
# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Environment	= YACC=/usr/local/bin/bison-1.28 \
			php_cv_bison_version=1.28 \
			LEX=/usr/local/bin/lex-2.5.4 \
			MAKEOBJDIR="$(BuildDirectory)" \
			INSTALL_ROOT="$(DSTROOT)" \
			TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)"
# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment	= CFLAGS="$$RC_CFLAGS -Os" \
					LDFLAGS="$$RC_CFLAGS -Os" \
					EXTRA_LIBS="-lresolv"

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
			--with-xmlrpc \
				--with-iconv-dir=/usr \
			--with-xsl=/usr
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
# The FreeType2 library is only installed on Snow Leopard.
# ... but it requires CoreServices and ApplicationServices frameworks. Bad!
#ifneq ($(strip $(wildcard /usr/local/include/freetype2/*)),)
#Extra_Configure_Flags	+= --with-freetype-dir=/usr/local
#endif

Dependencies	= libjpeg libpng
# Let this Makefile manage source copying.
CommonNoInstallSource	= YES

# Additional project info used with AEP
AEP		= YES
AEP_Version	= 5.3.0
AEP_LicenseFile	= $(Sources)/LICENSE
AEP_Patches	= MacOSX_build.patch force_dlfcn.patch arches.patch \
				NLS_remove_BIND8.patch iconv.patch \
				where_is_pcre.patch \
				mysql_sock.patch
AEP_ConfigDir	= $(ETCDIR)

# Used only in this file
PROJECT_FILES	= Makefile AEP.make $(ProjectName).plist 

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

install-macosx:
	@echo "Cleaning up install for Mac OS X..."
	-$(RMDIR) $(DSTROOT)$(ETCDIR)/apache2
	$(CHOWN) -R root:wheel $(DSTROOT)/
	$(INSTALL_FILE) $(Sources)/php.ini-production $(DSTROOT)$(AEP_ConfigDir)/php.ini.default
	$(PERL) -i -pe 's|^extension_dir =.*|extension_dir = /usr/lib/php/extensions/no-debug-non-zts-20060613|' $(DSTROOT)$(AEP_ConfigDir)/php.ini.default
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/lib/php/extensions/no-debug-non-zts-20060613
	@echo "Removing references to DSTROOT in php-config..."
	$(CP) $(DSTROOT)$(USRBINDIR)/php-config $(SYMROOT)/php-config && $(SED) -e 's=-L$(DSTROOT)/usr/local/lib==' $(SYMROOT)/php-config | $(SED) -e 's@$(DSTROOT)@@g' > $(DSTROOT)$(USRBINDIR)/php-config
	@echo "Archiving and stripping binaries..."
	if [ ! -d $(SYMROOT) ]; then \
		$(MKDIR) -m 755 $(SYMROOT); \
	fi
	$(_v) for file in "$(DSTROOT)`/usr/sbin/apxs -q LIBEXECDIR`/"*.so $(DSTROOT)/usr/bin/php;	\
	do \
		$(CP) $${file} $(SYMROOT);	\
		$(STRIP) -S $${file};		\
	done
	@echo "Deleting private dependencies..."
	-$(RMDIR) $(DSTROOT)/usr/local/lib
	-$(RMDIR) $(DSTROOT)/usr/local/include
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
