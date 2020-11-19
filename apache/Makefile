Project    = httpd

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Sources    = $(SRCROOT)/$(Project)

Patch_List = PR-18640257-SDK.diff \
             patch-config.layout \
             patch-configure \
             PR-3921505.diff \
             PR-5432464.diff_httpd.conf \
             PR-16019492.diff \
             PR-5957348.diff \
             patch-docs__conf__httpd.conf.in \
             PR-24076433 \
             apachectl.diff \
             patch-docs__conf__extra__httpd-mpm.conf.in \
             patch-docs__conf__mime.types \
             patch-docs__conf__extra__httpd-userdir.conf.in \
             PR-15976165-ulimit.diff \
             PR-17754441-sbin.diff \
             PR-16019357-apxs.diff \
             PR-13708279.diff \
             PR-47162596.diff \
             OpenSSLVersion.diff \
             man.diff \
             patch-server__Makefile.in 

Configure_Flags = CPPFLAGS="-iwithsysroot /usr/local/libressl/include" \
                  LDFLAGS="-L${SDKROOT}/usr/local/libressl/lib" \
                  --prefix=/usr \
                  --enable-layout=Darwin \
                  --with-apr=$(shell xcrun -find apr-1-config) \
                  --with-apr-util=$(shell xcrun -find apu-1-config) \
                  --with-pcre=$(SRCROOT)/$(Project)/../\
                  --enable-mods-shared=all \
                  --enable-ssl \
                  --with-z=$(SDKROOT)/usr/include \
                  --with-libxml2=$(SDKROOT)/usr/include/libxml2 \
                  --with-ssl=$(SDKROOT)/usr/local/libressl \
                  --enable-cache \
                  --enable-mem-cache \
                  --enable-proxy-balancer \
                  --enable-proxy \
                  --enable-proxy-http \
                  --enable-disk-cache \
                  --enable-mpms-shared=all \
                  --with-mpm=prefork \
                  --enable-cgi \
                  --enable-imagemap \
                  --enable-negotiation \
                  --enable-slotmem-shm

Post_Install_Targets = module-setup module-disable module-enable recopy-httpd-conf \
                       post-install strip-modules

# Extract the source.

install_source:: 
	for patch in $(Patch_List); do \
		(cd $(Sources) && patch -p0 -i $(SRCROOT)/patches/$${patch}) || exit 1; \
	done

build::
	$(MKDIR) $(OBJROOT)
	cd $(BuildDirectory) && $(Sources)/configure $(Configure_Flags)
	cd $(BuildDirectory) && make EXTRA_CFLAGS="$(RC_CFLAGS) -framework CoreFoundation -D_FORTIFY_SOURCE=2" MOD_LDFLAGS="-L$(SDKROOT)/usr/local/libressl/lib -lcrypto -lssl" HTTPD_LDFLAGS="-sectcreate __TEXT __info_plist  $(SRCROOT)/Info.plist"


install::
	cd $(BuildDirectory) && make install DESTDIR=$(DSTROOT)
	$(_v) $(MAKE) $(Post_Install_Targets)

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
	$(RM) $(DSTROOT)/Library/WebServer/CGI-Executables/*
	$(INSTALL_FILE) $(SRCROOT)/checkgid.8 $(DSTROOT)/usr/share/man/man8
	$(INSTALL_FILE) $(SRCROOT)/httpd-wrapper.8 $(DSTROOT)/usr/share/man/man8
	$(CHOWN) -R $(Install_User):$(Install_Group) \
		$(DSTROOT)/usr/share/httpd \
		$(DSTROOT)/usr/share/man
	$(MV) $(DSTROOT)/Library/WebServer/Documents/index.html $(DSTROOT)/Library/WebServer/Documents/index.html.en
	$(INSTALL_FILE) $(SRCROOT)/PoweredByMacOSX*.gif $(DSTROOT)/Library/WebServer/Documents
	$(MKDIR) $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL_SCRIPT) $(SRCROOT)/httpd-wrapper.rb $(DSTROOT)/usr/sbin/httpd-wrapper
	$(INSTALL_FILE) $(SRCROOT)/org.apache.httpd.plist $(DSTROOT)/System/Library/LaunchDaemons
	$(MKDIR) $(DSTROOT)/usr/local/OpenSourceVersions $(DSTROOT)/usr/local/OpenSourceLicenses
	$(INSTALL_FILE) $(SRCROOT)/apache.plist $(DSTROOT)/usr/local/OpenSourceVersions/apache.plist
	$(INSTALL_FILE) $(Sources)/LICENSE $(DSTROOT)/usr/local/OpenSourceLicenses/apache.txt
	$(MKDIR) $(DSTROOT)$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin
	$(MKDIR) $(DSTROOT)$(TOOLCHAIN_INSTALL_DIR)/usr/share/httpd/build
	$(MKDIR) $(DSTROOT)$(TOOLCHAIN_INSTALL_DIR)/usr/share/man/man1
	$(MKDIR) $(DSTROOT)/usr/local/bin
	$(MV) $(DSTROOT)/usr/sbin/apxs $(DSTROOT)/usr/local/bin/apxs
	sed -e "/get_config_vars/s,\".installbuilddir,\"$(TOOLCHAIN_INSTALL_DIR)/usr/share/httpd/build," < $(DSTROOT)/usr/local/bin/apxs > $(DSTROOT)$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin/apxs
	$(CHMOD) 0755 $(DSTROOT)$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin/apxs
	sed -e "55s,.*,includedir = $(SDKROOT)/usr/include/apache2," -e 's,/AppleInternal/BuildRoot,,g' < $(DSTROOT)/usr/share/httpd/build/config_vars.mk > $(DSTROOT)$(TOOLCHAIN_INSTALL_DIR)/usr/share/httpd/build/config_vars.mk
	$(RM) $(DSTROOT)/usr/share/httpd/build/config_vars.mk
	$(RM) $(DSTROOT)/usr/share/httpd/build/config.nice
	$(RM) $(DSTROOT)/usr/local/bin/apxs
	$(MV) $(DSTROOT)/usr/share/man/man1/apxs.1 $(DSTROOT)$(TOOLCHAIN_INSTALL_DIR)/usr/share/man/man1
	/usr/bin/find  $(DSTROOT)/usr/share/httpd/icons $(DSTROOT)/usr/share/httpd/error $(DSTROOT)/usr/share/man -type f -exec chmod 0444 {} \;
	$(CHMOD) 0755 $(DSTROOT)/usr/share/httpd/icons/small
	$(CHMOD) 0755 $(DSTROOT)/usr/share/httpd/error/include
	$(SILENT) $(RM) -Rf $(DSTROOT)/BuildRoot
	$(SILENT) $(RM) -Rf $(DSTROOT)/AppleInternal/BuildRoot
	/usr/bin/codesign --force --sign - --entitlements $(SRCROOT)/entitlements.plist $(DSTROOT)/usr/sbin/httpd
	
strip-modules:
	$(CP) $(DSTROOT)/usr/libexec/apache2/*.so $(SYMROOT)
	$(STRIP) -S $(DSTROOT)/usr/libexec/apache2/*.so
