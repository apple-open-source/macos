Project    = httpd

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Version    = 2.4.16
Sources    = $(SRCROOT)/$(Project)

Patch_List = patch-config.layout \
             patch-configure \
             PR-3921505.diff \
             PR-5432464.diff_httpd.conf \
             PR-16019492.diff \
             PR-5957348.diff \
             patch-docs__conf__httpd.conf.in \
             PR-10154185.diff \
             apachectl.diff \
             patch-docs__conf__extra__httpd-mpm.conf.in \
             patch-docs__conf__mime.types \
             patch-docs__conf__extra__httpd-userdir.conf.in \
             PR-15976165-ulimit.diff \
             PR-17754441-sbin.diff \
             PR-16019357-apxs.diff \
             mod_proxy_balancer-partialfix.diff

Configure_Flags = --prefix=/usr \
                  --enable-layout=Darwin \
                  --with-apr=/usr \
                  --with-apr-util=/usr \
                  --with-pcre=$(SRCROOT)/$(Project)/../\
                  --enable-mods-shared=all \
                  --enable-ssl \
                  --enable-cache \
                  --enable-mem-cache \
                  --enable-proxy-balancer \
                  --enable-proxy \
                  --enable-proxy-http \
                  --enable-disk-cache \
                  --with-mpm=prefork \
                  --enable-imagemap \
                  --enable-negotiation \
                  --enable-slotmem-shm

Post_Install_Targets = module-setup module-disable module-enable recopy-httpd-conf \
                       post-install strip-modules

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(Project)-$(Version).tar.bz2
	$(RMDIR) $(Sources)
	$(MV) $(SRCROOT)/$(Project)-$(Version) $(Sources)
	for patch in $(Patch_List); do \
		(cd $(Sources) && patch -p0 -i $(SRCROOT)/patches/$${patch}) || exit 1; \
	done

build::
	$(MKDIR) $(OBJROOT)
	cd $(BuildDirectory) && $(Sources)/configure $(Configure_Flags)
	cd $(BuildDirectory) && make EXTRA_CFLAGS="$(RC_CFLAGS) -D_FORTIFY_SOURCE=2"

install::
	cd $(BuildDirectory) && make install DESTDIR=$(DSTROOT)
	$(_v) $(MAKE) $(Post_Install_Targets)

APXS = perl $(OBJROOT)/support/apxs
SYSCONFDIR = /private/etc/apache2
SYSCONFDIR_OTHER = $(SYSCONFDIR)/other

## XXX: external modules should install their own config files
module-setup:
	$(MKDIR) $(DSTROOT)$(SYSCONFDIR_OTHER)
	$(INSTALL_FILE) $(SRCROOT)/conf/*.conf $(DSTROOT)$(SYSCONFDIR_OTHER)
	$(RM) $(DSTROOT)$(SYSCONFDIR)/httpd.conf.bak

# 4831254
module-disable:
	sed -i '' -e '/unique_id_module/s/^/#/' $(DSTROOT)$(SYSCONFDIR)/httpd.conf
	sed -i '' -e '/lbmethod_heartbeat_module/s/^/#/' $(DSTROOT)$(SYSCONFDIR)/httpd.conf

# enable negotiation_module for Multiviews on files: .en, .de, .fr, etc
module-enable:
	sed -i '' -e '/negotiation_module/s/^#//' $(DSTROOT)$(SYSCONFDIR)/httpd.conf

# 6927748: This needs to run after we're done processing httpd.conf (and anything in extra)
recopy-httpd-conf:
	cp $(DSTROOT)$(SYSCONFDIR)/httpd.conf $(DSTROOT)$(SYSCONFDIR)/original/httpd.conf

post-install:
	$(MKDIR) $(DSTROOT)$(SYSCONFDIR)/users
	$(RMDIR) $(DSTROOT)/private/var/run
	$(RMDIR) $(DSTROOT)/usr/bin
	$(RM) $(DSTROOT)/Library/WebServer/CGI-Executables/printenv
	$(RM) $(DSTROOT)/Library/WebServer/CGI-Executables/test-cgi
	$(INSTALL_FILE) $(SRCROOT)/checkgid.8 $(DSTROOT)/usr/share/man/man8
	$(INSTALL_FILE) $(SRCROOT)/httxt2dbm.8 $(DSTROOT)/usr/share/man/man8
	$(CHOWN) -R $(Install_User):$(Install_Group) \
		$(DSTROOT)/usr/share/httpd \
		$(DSTROOT)/usr/share/man
	$(MV) $(DSTROOT)/Library/WebServer/Documents/index.html $(DSTROOT)/Library/WebServer/Documents/index.html.en
	$(INSTALL_FILE) $(SRCROOT)/PoweredByMacOSX*.gif $(DSTROOT)/Library/WebServer/Documents
	$(MKDIR) $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL_SCRIPT) $(SRCROOT)/webpromotion.rb $(DSTROOT)/usr/sbin/webpromotion
	$(INSTALL_SCRIPT) $(SRCROOT)/httpd-wrapper.rb $(DSTROOT)/usr/sbin/httpd-wrapper
	$(INSTALL_FILE) $(SRCROOT)/org.apache.httpd.plist $(DSTROOT)/System/Library/LaunchDaemons
	$(MKDIR) $(DSTROOT)/usr/local/OpenSourceVersions $(DSTROOT)/usr/local/OpenSourceLicenses
	$(INSTALL_FILE) $(SRCROOT)/apache.plist $(DSTROOT)/usr/local/OpenSourceVersions/apache.plist
	$(INSTALL_FILE) $(Sources)/LICENSE $(DSTROOT)/usr/local/OpenSourceLicenses/apache.txt

strip-modules:
	$(CP) $(DSTROOT)/usr/libexec/apache2/*.so $(SYMROOT)
	$(STRIP) -S $(DSTROOT)/usr/libexec/apache2/*.so
