##
# Makefile for php4. 
# This is for the legacy Apache 1.3 installation on the server, so the install-cleanup
# step removes everything except the Apache plugin.
##

# Project info
Project               = php
UserType              = Administration
ToolType              = Services
# We want these, but can't for various reasons right now:
# --with-xmlrpc --with-expat-dir=/usr --with-cyrus=/usr --with-gd
Extra_LD_Flags	      = -lresolv
Extra_Configure_Flags = --with-apxs=/usr/sbin/apxs-1.3 --with-ldap=/usr --with-kerberos=/usr --enable-cli --with-zlib-dir=/usr --enable-trans-sid --with-xml --enable-exif --enable-ftp --enable-mbstring --enable-mbregex --enable-dbx --enable-sockets --with-iodbc=/usr --with-curl=/usr --with-config-file-path=/etc --sysconfdir=$(ETCDIR) \
                        --with-mysql=/usr --with-mysql-sock=/var/mysql/mysql.sock
Environment += LEX=/usr/local/bin/lex-2.5.4

GnuAfterInstall       = strip mode install-ini install-plist install-cleanup

Framework = $(NSFRAMEWORKDIR)/php.framework/Versions/4

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install
Install_Flags  = INSTALL_ROOT=$(DSTROOT)

strip:
	$(_v) $(STRIP) -S "$(DSTROOT)`apxs-1.3 -q LIBEXECDIR`/"*.so
	$(_v) $(STRIP) $(DSTROOT)/usr/bin/php
	$(_v) $(RM) $(DSTROOT)/usr/lib/php/.lock

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
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project)4.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(Project)4.txt

install-cleanup:
	$(RM) -rf $(DSTROOT)/private
	$(RM) -rf $(DSTROOT)/usr/bin
	$(RM) -rf $(DSTROOT)/usr/include
	$(RM) -rf $(DSTROOT)/usr/lib
	$(RM) -rf $(DSTROOT)/usr/share
	$(RM) -rf $(DSTROOT)/.channels $(DSTROOT)/.depdb $(DSTROOT)/.registry $(DSTROOT)/.lock $(DSTROOT)/.filemap $(DSTROOT)/.depdblock

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 4.4.9
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = TSRM__build.mk.diff TSRM__buildconf.diff \
                 Zend__build.mk.diff Zend__buildconf.diff \
                 ext__mbstring__libmbfl__buildconf.diff \
                 ext__mbstring__libmbfl__config.h.diff \
		 NLS_remove_BIND8.patch \
                 Makefile.diff

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
	perl -i -pe 's|-i -a -n php4|-i -n php4|g' $(SRCROOT)/$(Project)/configure
endif
