##
# Makefile for php
##

# Project info
Project               = php
UserType              = Administration
ToolType              = Services
# We want these, but can't for various reasons right now:
# --with-xmlrpc --with-expat-dir=/usr --with-cyrus=/usr --with-gd
Extra_Configure_Flags = --with-apxs --with-ldap=/usr --with-kerberos=/usr --enable-cli --with-zlib-dir=/usr --enable-trans-sid --with-xml --enable-exif --enable-ftp --enable-mbstring --enable-dbx --enable-sockets --with-iodbc=/usr --with-curl=/usr --with-config-file-path=/etc
Extra_Environment     = AR="$(SRCROOT)/ar.sh" PEAR_INSTALLDIR="$(NSLIBRARYDIR)/PHP"
GnuAfterInstall       = strip mode install-ini

Framework = $(NSFRAMEWORKDIR)/php.framework/Versions/4

# It's a GNU Source project
Extra_CC_Flags = -no-cpp-precomp -DBIND_8_COMPAT
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
Extra_CC_Flags = -no-cpp-precomp -DBIND_8_COMPAT

Install_Target = install

Install_Flags = INSTALL_ROOT=$(DSTROOT)

build:: makehttpdconf

makehttpdconf:
	$(MKDIR) -p $(DSTROOT)/private/etc/httpd
	$(CP) /etc/httpd/httpd.conf $(DSTROOT)/private/etc/httpd/httpd.conf

strip:
	$(_v) $(STRIP) -S "$(DSTROOT)`apxs -q LIBEXECDIR`/"*.so
	$(_v) $(STRIP) $(DSTROOT)/usr/bin/php
	$(_v) $(RM) $(DSTROOT)/usr/lib/php/.lock
	$(RM) $(DSTROOT)/private/etc/httpd/httpd.conf
	$(RM) $(DSTROOT)/private/etc/httpd/httpd.conf.bak

mode:
	$(_v) $(CHMOD) -R ugo-w "$(DSTROOT)"

install-ini:
	install -d $(DSTROOT)/private/etc
	install -m 644 $(SRCROOT)/php/php.ini-dist $(DSTROOT)/private/etc/php.ini.default
