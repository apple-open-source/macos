##
# Makefile for apache
##
# Wilfredo Sanchez | wsanchez@apple.com
##

# Project info
Project         = apache
UserType        = Administration
ToolType        = Services
GnuAfterInstall = install-local

# It's a GNU Source project
# Well, not really but we can make it work.
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Ignore RC_CFLAGS
Extra_CC_Flags = -DHARD_SERVER_LIMIT=1024

Environment =

# We put CFLAGS and LDFLAGS into the configure environment directly,
# and not in $(Environment), because the Apache Makefiles don't follow
# GNU guidelines, though configure mostly does.

Documentation    = $(NSDOCUMENTATIONDIR)/$(ToolType)/apache
Install_Flags    = root="$(DSTROOT)"			\
		      sysconfdir=$(ETCDIR)/httpd	\
		   Localstatedir=$(VARDIR)		\
		      runtimedir=$(VARDIR)/run		\
		      logfiledir=$(VARDIR)/log/httpd	\
		   proxycachedir=$(VARDIR)/run/proxy

Install_Target   = install
Configure_Flags  = --enable-shared=max	\
		   --enable-module=most

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
# We want to compile all of the modules, so users don't have to, but
# we don't want them all turned on by default, since they add bloat
# to the server and hinder performance.
# Let's disable the ones users aren't likely to need, while leaving
# them the option of re-enabling them as desired.
# Modules listed here are disabled, except modules preceded by '-'
# are not disabled--a hack so we can keep a full list of modules here.
##
Disabled_Modules = 				\
		   vhost_alias:mod_vhost_alias	\
		   env:mod_env			\
		  -config_log:mod_log_config	\
		   mime_magic:mod_mime_magic	\
		  -mime:mod_mime		\
		  -negotiation:mod_negotiation	\
		   status:mod_status		\
		   info:mod_info		\
		  -includes:mod_include		\
		  -autoindex:mod_autoindex	\
		  -dir:mod_dir			\
		  -cgi:mod_cgi			\
		  -asis:mod_asis		\
		  -imap:mod_imap		\
		  -action:mod_actions		\
		   speling:mod_speling		\
		  -userdir:mod_userdir		\
		  -alias:mod_alias		\
		  -rewrite:mod_rewrite		\
		  -access:mod_access		\
		  -auth:mod_auth		\
		   anon_auth:mod_auth_anon	\
		   dbm_auth:mod_auth_dbm	\
		   digest:mod_digest		\
		   proxy:mod_proxy		\
		   cern_meta:mod_cern_meta	\
		   expires:mod_expires		\
		   headers:mod_headers		\
		   usertrack:mod_usertrack	\
		   unique_id:mod_unique_id	\
		  -setenvif:mod_setenvif

##
# These modules are build separately, but we want to include them in
# the default config file.
##
External_Modules = dav:libdav	\
		   ssl:libssl

##
# install-local does the following:
# - Install our default doc root.
# - Install our version of printenv. (Need to resubmit to Apache.)
# - Move apache manual to documentation directory, place a symlink to it
#   in the doc root.
# - Add a symlink to the Apache release note in the doc root.
# - Make the server root group writeable.
# - Disable non-"standard" modules.
# - Add (disabled) external modules.
# - Edit the configuration defaults as needed.
# - Remove -arch foo flags from apxs since module writers may not build
#   for the same architectures(s) as we do.
##

APXS_DST = $(DSTROOT)$(USRSBINDIR)/apxs

LocalWebServer = $(NSLOCALDIR)$(NSLIBRARYSUBDIR)/WebServer
ConfigDir      = /private/etc/httpd
ConfigFile     = $(ConfigDir)/httpd.conf
DocRoot        = $(LocalWebServer)/Documents
CGIDir         = $(LocalWebServer)/CGI-Executables

APXS = $(APXS_DST) -e				\
	-S SBINDIR="$(DSTROOT)$(USRSBINDIR)"	\
	-S SYSCONFDIR="$(DSTROOT)$(ConfigDir)"

install-local:
	@echo "Fixing up documents"
	$(_v) $(INSTALL_FILE) -c -m 664 "$(SRCROOT)/DocumentRoot/"*.gif "$(DSTROOT)$(DocRoot)"
	$(_v) $(INSTALL_FILE) -c -m 664 printenv "$(DSTROOT)$(CGIDir)"
	$(_v) $(MKDIR) `dirname "$(DSTROOT)$(Documentation)"`
	$(_v) $(RMDIR) "$(DSTROOT)$(Documentation)"
	$(_v) $(MV) "$(DSTROOT)$(DocRoot)/manual" "$(DSTROOT)$(Documentation)"
	$(_v) $(LN) -fs "$(Documentation)" "$(DSTROOT)$(DocRoot)/manual"
	$(_v) $(CHMOD) -R g+w "$(DSTROOT)$(DocRoot)"
	$(_v) $(CHMOD) -R g+w "$(DSTROOT)$(CGIDir)"
	@echo "Fixing up configuration"
	$(_v) perl -i -pe 's|-arch\s+\S+\s*||g' $(DSTROOT)$(USRSBINDIR)/apxs
	$(_v) $(CP) $(DSTROOT)$(ConfigFile).default $(DSTROOT)$(ConfigFile)
	$(_v) for mod in $(Disabled_Modules); do								\
		  if ! (echo $${mod} | grep -e '^-' > /dev/null); then						\
		      module=$${mod%:*};									\
		        file=$${mod#*:};									\
	              perl -i -pe 's|^(LoadModule\s+'$${module}'_module\s+)|#$${1}|' $(DSTROOT)$(ConfigFile);	\
	              perl -i -pe 's|^(AddModule\s+'$${file}'\.c)$$|#$${1}|'         $(DSTROOT)$(ConfigFile);	\
		  fi;												\
	      done
	$(_v) for mod in $(External_Modules); do	\
		  module=$${mod%:*};			\
		    file=$${mod#*:};			\
		  $(APXS) -A -n $${module} $${file}.so;	\
	      done
	$(_v) perl -i -pe 's|(User\s+).*$$|$${1}www|'							$(DSTROOT)$(ConfigFile)
	$(_v) perl -i -pe 's|(Group\s+).*$$|$${1}www|'							$(DSTROOT)$(ConfigFile)
	$(_v) perl -i -pe 's|(MinSpareServers\s+)\d+$$|$${1}1|'						$(DSTROOT)$(ConfigFile)
	$(_v) perl -i -pe 's|(MaxSpareServers\s+)\d+$$|$${1}5|'						$(DSTROOT)$(ConfigFile)
	$(_v) perl -i -pe 's|(StartServers\s+)\d+$$|$${1}1|'						$(DSTROOT)$(ConfigFile)
	$(_v) perl -i -pe 's|(MaxRequestsPerChild\s+)\d+$$|$${1}100000|'				$(DSTROOT)$(ConfigFile)
	$(_v) perl -i -pe 's|(UserDir\s+).+$$|$${1}\"Sites\"|'						$(DSTROOT)$(ConfigFile)
	$(_v) perl -i -pe 's|(ServerAdmin\s+).*$$|#$${1}webmaster\@example.com|'			$(DSTROOT)$(ConfigFile)
	$(_v) perl -i -pe 's|(ServerName\s+).*$$|#$${1}www.example.com|'				$(DSTROOT)$(ConfigFile)
	$(_v) perl -i -pe 's|Log "(/var/log/httpd/.+)"|Log "\|/usr/sbin/rotatelogs $${1} 86400"|'	$(DSTROOT)$(ConfigFile)
	$(_v) echo "" >>										$(DSTROOT)$(ConfigFile)
	$(_v) echo "Include $(ConfigDir)/users" >>							$(DSTROOT)$(ConfigFile)
	$(_v) $(CP)    $(DSTROOT)$(ConfigFile) $(DSTROOT)$(ConfigFile).default
	$(_v) $(RM)    $(DSTROOT)$(ConfigDir)/access.conf*
	$(_v) $(RM)    $(DSTROOT)$(ConfigDir)/srm.conf*
	$(_v) $(MKDIR) $(DSTROOT)$(ConfigDir)/users
