##
# Makefile for apache1
##
# Wilfredo Sanchez | wsanchez@apple.com
##

# Project info
Project         = apache1
UserType        = Administration
ToolType        = Services
GnuAfterInstall = install-local install-plist

# It's a GNU Source project
# Well, not really but we can make it work.
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = apache1
AEP_Version    = 1.3.41
AEP_ProjVers   = apache_$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = NLS_current_apache.patch \
                 NLS_PR-3995868.patch PR-4415227.patch \
                 httpd_path.patch \
		 apachectl.patch \
		 launchd.patch

#mod_ssl
mod_ssl_Project = apache_mod_ssl
AEP_mod_ssl_Project = mod_ssl
AEP_mod_ssl_Version = 2.8.31
AEP_mod_ssl_ProjVers   = $(AEP_mod_ssl_Project)-$(AEP_mod_ssl_Version)-$(AEP_Version)
AEP_mod_ssl_Filename   = $(AEP_mod_ssl_ProjVers).tar.gz
AEP_mod_ssl_ExtractDir = $(AEP_mod_ssl_ProjVers)
AEP_mod_ssl_Patches    = NLS_mod_ssl_curent.patch


ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	
	#apache stage	

	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(AEP_Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done

	#mod_ssl stage

	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(mod_ssl_Project)/$(AEP_mod_ssl_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_mod_ssl_Project)
	$(MV) $(SRCROOT)/$(AEP_mod_ssl_ExtractDir) $(SRCROOT)/$(AEP_mod_ssl_Project)
	for patchfile in $(AEP_mod_ssl_Patches); do \
		cd $(SRCROOT)/$(AEP_mod_ssl_Project) && patch -p0 < $(SRCROOT)/$(mod_ssl_Project)/$(AEP_mod_ssl_Project)_patches/$$patchfile; \
	done
	$(RMDIR) $(SRCROOT)/$(mod_ssl_Project)
	
endif


# Ignore RC_CFLAGS
Extra_CC_Flags = -DHARD_SERVER_LIMIT=2048 

Environment =

# We put CFLAGS and LDFLAGS into the configure environment directly,
# and not in $(Environment), because the Apache Makefiles don't follow
# GNU guidelines, though configure mostly does.

Documentation    = $(NSDOCUMENTATIONDIR)/$(ToolType)/apache-1.3
Install_Flags    = root="$(DSTROOT)"			\
		      sysconfdir=$(ETCDIR)/httpd	\
		   Localstatedir=$(VARDIR)		\
		      runtimedir=$(VARDIR)/run		\
		      logfiledir=$(VARDIR)/log/httpd	\
		        iconsdir=$(SHAREDIR)/httpd/icons \
		     includedir=$(USRINCLUDEDIR)/httpd \
		      libexecdir=$(LIBEXECDIR)/httpd  \
		   proxycachedir=$(VARDIR)/run/proxy-1.3

Install_Target   = install
Configure_Flags  = --enable-shared=max	\
		   --enable-module=most \
		   --sysconfdir=$(ETCDIR)/httpd \
		   --target=httpd-1.3

ifeq ($(wildcard mod_ssl),)
Configure        = cd $(shell pwd)/apache && CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" $(Sources)/configure
Configure_Flags += --shadow="$(BuildDirectory)"
else
Configure        = CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" "$(BuildDirectory)/configure"

# We don't want to put the EAPI patches into the source tree because we
# don't want to sift through them all the time, so lets's patch the source
# tree before each build.
Extra_CC_Flags += -DEAPI

lazy_install_source:: install-patched-source

install-patched-source:
	$(_v) if [ ! -f "$(BuildDirectory)/configure" ]; then					\
		  echo "Copying source for $(Project)...";					\
		  $(MKDIR) "$(BuildDirectory)";							\
		  (cd $(Sources) && $(PAX) -rw . "$(BuildDirectory)");				\
		  $(PAX) -rw mod_ssl "$(BuildDirectory)";					\
		  echo "Patching source (add EAPI) for $(Project)...";				\
		  (cd "$(BuildDirectory)/mod_ssl" &&						\
		  ./configure --with-apache="$(BuildDirectory)" --with-eapi-only);		\
	      fi

endif

##
# These modules are build separately, but we want to include them in
# the default config file.
##
External_Modules = dav:libdav	\
		   ssl:libssl	\
		   perl:libperl	\
		   php4:libphp4

##
# install-local does the following:
# - Install our default doc root.
# - Move apache manual to documentation directory, place a symlink to it
#   in the doc root.
# - Add a symlink to the Apache release note in the doc root.
# - Make the server root group writeable.
# - Remove -arch foo flags from apxs since module writers may not build
#   for the same architectures(s) as we do.
# - Install manpage for checkgid(1).
##

APXS_DST = $(DSTROOT)$(USRSBINDIR)/apxs-1.3

LocalWebServer = $(NSLOCALDIR)$(NSLIBRARYSUBDIR)/WebServer
ConfigDir      = /private/etc/httpd
ProxyDir       = /private/var/run/proxy-1.3
ConfigFile     = $(ConfigDir)/httpd.conf
DocRoot        = $(LocalWebServer)/Documents
CGIDir         = $(LocalWebServer)/CGI-Executables

APXS = $(APXS_DST) -e				\
	-S SBINDIR="$(DSTROOT)$(USRSBINDIR)"	\
	-S SYSCONFDIR="$(DSTROOT)$(ConfigDir)"

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(Project).txt

install-local:
	@echo "Munging paths"
	@for bin in checkgid dbmmanage htdigest htpasswd; do \
		$(MV) $(DSTROOT)/usr/bin/$${bin} $(DSTROOT)/usr/bin/$${bin}-1.3; \
		$(MV) $(DSTROOT)/usr/share/man/man1/$${bin}.1 $(DSTROOT)/usr/share/man/man1/$${bin}-1.3.1; \
	done
	$(MV) $(DSTROOT)/usr/sbin/httpd-1.3ctl $(DSTROOT)/usr/sbin/apachectl-1.3
	$(MV) $(DSTROOT)/usr/share/man/man8/httpd-1.3ctl.8 $(DSTROOT)/usr/share/man/man8/apachectl-1.3.8
	@for sbin in ab apxs logresolve rotatelogs; do \
		$(MV) $(DSTROOT)/usr/sbin/$${sbin} $(DSTROOT)/usr/sbin/$${sbin}-1.3; \
		$(MV) $(DSTROOT)/usr/share/man/man8/$${sbin}.8 $(DSTROOT)/usr/share/man/man8/$${sbin}-1.3.8; \
	done
	@echo "Fixing up documents"
	$(_v) $(MKDIR) `dirname "$(DSTROOT)$(Documentation)"`
	$(_v) $(RMDIR) "$(DSTROOT)$(Documentation)"
	$(_v) $(MV) "$(DSTROOT)$(DocRoot)/manual" "$(DSTROOT)$(Documentation)"
	$(_v) $(LN) -fs "$(Documentation)" "$(DSTROOT)$(DocRoot)/manual-1.3"
	$(_v) $(CHMOD) -R g+w "$(DSTROOT)$(DocRoot)"
	$(_v) $(CHOWN) -R www:www "$(DSTROOT)$(ProxyDir)"
	@echo "Fixing up configuration"
	$(_v) perl -i -pe 's|-arch\s+\S+\s*||g' $(DSTROOT)$(USRSBINDIR)/apxs-1.3
	$(_v) $(RM)    $(DSTROOT)$(ConfigFile).bak
	$(_v) $(RM)    $(DSTROOT)$(ConfigFile).default
	$(_v) $(RM)    $(DSTROOT)$(ConfigFile)
	$(_v) $(RM)    $(DSTROOT)$(ConfigDir)/access.conf*
	$(_v) $(RM)    $(DSTROOT)$(ConfigDir)/srm.conf*
	$(_v) $(RM)    $(DSTROOT)$(ConfigDir)/httpd-1.3.conf
	$(_v) $(RM)    $(DSTROOT)$(ConfigDir)/httpd-1.3.conf.default
	$(_v) $(RM)    $(DSTROOT)$(ConfigDir)/mime.types
	$(_v) $(RM)    $(DSTROOT)$(ConfigDir)/mime.types.default
	$(_v) perl -i -pe 's|httpd|httpd-1.3|' $(DSTROOT)/usr/share/man/man8/httpd-1.3.8
	$(_v) perl -i -pe 's|httpd-1.3.apache.org|httpd.apache.org|' $(DSTROOT)/usr/share/man/man8/httpd-1.3.8
	$(_v) perl -i -pe 's|/usr/local/apache/conf|/etc/httpd|' $(DSTROOT)/usr/share/man/man8/httpd-1.3.8
	$(_v) perl -i -pe 's|/usr/local/apache/logs|/var/log/httpd|' $(DSTROOT)/usr/share/man/man8/httpd-1.3.8
	$(_v) perl -i -pe 's|/var/log/httpd/httpd-1.3.pid|/var/run/httpd-1.3.pid|' $(DSTROOT)/usr/share/man/man8/httpd-1.3.8
	$(_v) $(RM) -rf $(DSTROOT)/Library/WebServer/CGI-Executables/
	$(_v) $(RM) $(DSTROOT)/Library/WebServer/Documents/apache_pb.gif
	$(_v) $(RM) $(DSTROOT)/Library/WebServer/Documents/index.html.*
	$(_v) $(RM) -rf $(DSTROOT)/$(SHAREDIR)/httpd/icons
	$(_v) $(INSTALL_FILE) $(SRCROOT)/checkgid.1 \
		$(DSTROOT)/usr/share/man/man1/checkgid-1.3.1
	$(_v) $(MKDIR) -p $(DSTROOT)/System/Library/LaunchDaemons
	$(_v) $(INSTALL_FILE) -m 644 $(SRCROOT)/org.apache.httpd-1.3.plist $(DSTROOT)/System/Library/LaunchDaemons
