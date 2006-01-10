##
# Makefile for php
##

# Project info
Project               = php
UserType              = Administration
ToolType              = Services
# We want these, but can't for various reasons right now:
# --with-xmlrpc --with-expat-dir=/usr --with-cyrus=/usr --with-gd
Extra_LD_Flags	      = -lresolv
Extra_Configure_Flags = --with-apxs --with-ldap=/usr --with-kerberos=/usr --enable-cli --with-zlib-dir=/usr --enable-trans-sid --with-xml --enable-exif --enable-ftp --enable-mbstring --enable-mbregex --enable-dbx --enable-sockets --with-iodbc=/usr --with-curl=/usr --with-config-file-path=/etc --sysconfdir=$(ETCDIR) \
                        --with-mysql=/usr --with-mysql-sock=/var/mysql/mysql.sock
Extra_CC_Flags        = -no-cpp-precomp
GnuAfterInstall       = strip mode install-ini install-plist

Framework = $(NSFRAMEWORKDIR)/php.framework/Versions/4

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install
Install_Flags  = INSTALL_ROOT=$(DSTROOT)

build:: makehttpdconf

makehttpdconf:
	$(MKDIR) $(DSTROOT)/private/etc/httpd
	$(CP) /etc/httpd/httpd.conf $(DSTROOT)/private/etc/httpd/httpd.conf

strip:
	$(_v) $(STRIP) -S "$(DSTROOT)`apxs -q LIBEXECDIR`/"*.so
	$(_v) $(STRIP) $(DSTROOT)/usr/bin/php
	$(_v) $(RM) $(DSTROOT)/usr/lib/php/.lock
	$(RMDIR) $(DSTROOT)/private/etc/httpd

mode:
	$(_v) $(CHMOD) -R ugo-w "$(DSTROOT)"

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
AEP_Version    = 4.3.11
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
endif
