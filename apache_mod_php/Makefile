##
# Makefile for php
##

# Add for fast cgi: --enable-fastcgi \ 
#--enable-discard-path \ 
#--enable-force-cgi-redirect 

# Project info
Project               = php
UserType              = Administration
ToolType              = Services
# We want these, but can't for various reasons right now:
# --with-xmlrpc --with-expat-dir=/usr --with-cyrus=/usr --with-gd
# --with-libedit apparently is for cli / cgi only
#  enable gd when libjpeg and libpng are available
# sqlite3 appears to require pdo support
Extra_LD_Flags	      = -lresolv
Environment += YACC=/usr/local/bin/bison-1.28 php_cv_bison_version=1.28 LEX=/usr/local/bin/lex-2.5.4
Extra_Configure_Flags =  \
			--with-apxs2=/usr/sbin/apxs \
			--with-ldap=/usr \
			--with-kerberos=/usr \
			--enable-cli \
			--with-zlib-dir=/usr \
			--enable-trans-sid \
			--with-xml \
			--enable-exif \
			--enable-ftp \
			--enable-mbstring \
			--enable-mbregex \
			--enable-dbx \
			--enable-sockets \
			--with-iodbc=/usr \
			--with-curl=/usr \
			--with-config-file-path=/etc \
			--sysconfdir=$(ETCDIR) \
			--with-mysql-sock=/var/mysql \
			--with-mysqli=/usr/bin/mysql_config \
			--with-mysql=/usr \
			--with-openssl \
			--with-xmlrpc \
			--with-xsl=/usr \
			--without-pear 

Extra_CC_Flags        = -no-cpp-precomp
GnuAfterInstall       = strip mode install-ini install-plist 

Framework = $(NSFRAMEWORKDIR)/php.framework/Versions/5

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install
Install_Flags  = INSTALL_ROOT=$(DSTROOT)

strip:
	$(_v) $(LIPO) -remove x86_64 -output $(DSTROOT)/usr/bin/php $(DSTROOT)/usr/bin/php
	$(_v) $(LIPO) -remove ppc64 -output $(DSTROOT)/usr/bin/php $(DSTROOT)/usr/bin/php
	$(_v) $(STRIP) -S "$(DSTROOT)`/usr/sbin/apxs -q LIBEXECDIR`/"*.so
	$(_v) $(STRIP) -S $(DSTROOT)/usr/bin/php
	$(_v) $(RM) $(DSTROOT)/usr/lib/php/.lock
	$(RMDIR) $(DSTROOT)/private/etc/apache2

mode:
	$(_v) $(CHMOD) -R ugo-w "$(DSTROOT)"

# patch php_config.h
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	cp ${OBJROOT}/main/php_config.h ${OBJROOT}/main/php_config.h.bak
	$(_v) ed - ${OBJROOT}/main/php_config.h < $(SRCROOT)/patches/main__php_config.h.ed
	$(_v) $(TOUCH) $(ConfigStamp2)

install-ini:
	$(MKDIR) $(DSTROOT)/private/etc
	$(INSTALL_FILE) $(SRCROOT)/php/php.ini-dist $(DSTROOT)/private/etc/php.ini.default

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 5.2.8
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = TSRM__build.mk.diff TSRM__buildconf.diff \
                 Zend__build.mk.diff Zend__buildconf.diff \
                 ext__mbstring__libmbfl__buildconf.diff \
                 ext__mbstring__libmbfl__config.h.diff \
		 NLS_remove_BIND8.patch

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
	perl -i -pe 's|-i -a -n php5|-i -n php5|g' $(SRCROOT)/$(Project)/configure
endif

