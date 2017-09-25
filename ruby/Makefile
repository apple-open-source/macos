##
# Makefile for ruby
##

FW_DIR		= $(NSFRAMEWORKDIR)/Ruby.framework
FW_VERSION_DIR	= $(FW_DIR)/Versions/$(VERSION)
LOCAL_FW_RES_DIR = $(EXTRAS_DIR)/framework_resources
SITEDIR		= /Library/Ruby/Site
USRGEMDIR	= /Library/Ruby/Gems/$(VERSION0)

Project		= ruby
UserType	= Developer
ToolType	= Commands
GnuAfterInstall = post-install install-plist install-irbrc install-rails-placeholder
GnuNoBuild	= YES

# ruby_atomic.h + LibreSSL
Extra_CC_Flags = -DHAVE_GCC_ATOMIC_BUILTINS -iwithsysroot /usr/local/libressl/include
Extra_LD_Flags = -L $(SDKROOT)/usr/local/libressl/lib
# don't use xcrun as xcrun_log will break configure -- keep it like this for rbconfig.rb
Extra_Configure_Environment =
comma := ,
space :=
space +=
Extra_Configure_Flags  = \
	--prefix=$(FW_VERSION_DIR)$(USRDIR) \
	--sysconfdir=$(SITEDIR) \
	--with-sitedir=$(SITEDIR) \
	--enable-shared \
	--with-arch=$(subst $(space),$(comma),$(RC_ARCHS)) \
	--with-out-ext=tk \
	ac_cv_func_getcontext=no \
	ac_cv_func_setcontext=no \
	ac_cv_func_utimensat=no \
	ac_cv_c_compiler_gnu=no \
	ac_cv_header_net_if_h=yes \
	av_cv_header_ifaddrs_h=yes \
	rb_cv_pri_prefix_long_long=ll \
	ac_cv_sizeof_struct_stat_st_size=SIZEOF_OFF_T \
	ac_cv_sizeof_struct_stat_st_blocks=SIZEOF_INT64_T \
	ac_cv_sizeof_struct_stat_st_ino=SIZEOF_UINT64_T

# Stupid xcrun_log!
Configure=$(SRCROOT)/_Configure

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target         = install install-doc
Install_Flags          = DESTDIR=$(DSTROOT)

EXTRAS_DIR = $(SRCROOT)/extras

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = $(shell /usr/libexec/PlistBuddy -c 'Print :OpenSourceVersion' $(AEP_Project).plist)
AEP_URL        = $(shell /usr/libexec/PlistBuddy -c 'Print :OpenSourceURL' $(AEP_Project).plist)
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(shell basename $(AEP_URL))
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = \
	ext_openssl_extconf.rb.diff \
	configure.diff \
	tool_config.guess.diff \
	tool_mkconfig.rb.diff \
	common.mk.diff \
	message_tracing_main.c.diff \
	lib_rubygems_defaults.rb.diff \
	getaddrinfo-test.diff

MAJOR     = $(shell echo $(AEP_Version) | cut -d. -f1)
MINOR     = $(shell echo $(AEP_Version) | cut -d. -f2)
#TEENY     = $(shell echo $(AEP_Version) | $(SED) -e 's:-p.*::' | cut -d. -f3)
VERSION   = $(MAJOR).$(MINOR)
VERSION0  = $(MAJOR).$(MINOR).0
#VERSION3  = $(MAJOR).$(MINOR).$(TEENY)

ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	cat $(SRCROOT)/patches/Makefile.append >> ${BuildDirectory}/Makefile
	@set -x && cd ${BuildDirectory} && arch_hdrdir=`$(MAKE) print_arch_hdrdir`
	$(_v) $(TOUCH) $(ConfigStamp2)

build:: configure
	$(INSTALL_DIRECTORY) $(SYMROOT)
	$(_v) $(MAKE) -C $(BuildDirectory) CC=$(shell xcrun -f clang) OBJCOPY=": noobjcopy" RUBY_CODESIGN="-"

post-install:
	$(INSTALL_DIRECTORY) $(DSTROOT)$(FW_VERSION_DIR)/Resources
	$(LN) -vfsh Versions/Current/Resources $(DSTROOT)/$(FW_DIR)/Resources
	$(INSTALL_FILE) $(LOCAL_FW_RES_DIR)/Info.plist $(DSTROOT)/$(FW_VERSION_DIR)/Resources
	$(INSTALL_FILE) $(LOCAL_FW_RES_DIR)/version.plist $(DSTROOT)/$(FW_VERSION_DIR)/Resources
	$(SED) -e "s:_CFBundleShortVersionString_:$(MACOSX_DEPLOYMENT_TARGET):" \
	    -e "s:_CFBundleVersion_:$(AEP_Version)-$(RC_ProjectSourceVersion):" \
	   $(LOCAL_FW_RES_DIR)/Info.plist > $(DSTROOT)/$(FW_VERSION_DIR)/Resources/Info.plist
	$(INSTALL_FILE) $(LOCAL_FW_RES_DIR)/version.plist \
		$(DSTROOT)/$(FW_VERSION_DIR)/Resources/version.plist
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(FW_VERSION_DIR)/Resources/English.lproj
	$(INSTALL_FILE) $(LOCAL_FW_RES_DIR)/English.lproj/InfoPlist.strings $(DSTROOT)/$(FW_VERSION_DIR)/Resources/English.lproj
	$(LN) -vfsh $(VERSION) $(DSTROOT)/$(FW_DIR)/Versions/Current
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(FW_VERSION_DIR)/Headers
	$(LN) -vfsh Versions/Current/Headers $(DSTROOT)/$(FW_DIR)/Headers
	$(LN) -vfh $(DSTROOT)$(FW_VERSION_DIR)$(USRINCLUDEDIR)/ruby-$(VERSION0)/ruby.h $(DSTROOT)/$(FW_DIR)/Headers/
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(FW_VERSION_DIR)/Headers/ruby
	# fix #include <ruby.h> for BridgeSupport
	for h in $(DSTROOT)$(FW_VERSION_DIR)$(USRINCLUDEDIR)/ruby-$(VERSION0)/*/*.h; do \
		$(SED) -i '' -e 's:#include <ruby\.h>:#include "ruby\.h":' \
			-e 's:const rb_data_type_t \*parent:const struct rb_data_type_struct \*parent:' $$h; \
		$(LN) -vfh $$h $(DSTROOT)/$(FW_VERSION_DIR)/Headers/ruby; \
	done
	$(LN) -vfh $(shell find $(DSTROOT)$(FW_VERSION_DIR)$(USRINCLUDEDIR)/ruby-$(VERSION0) -name config.h) $(DSTROOT)/$(FW_VERSION_DIR)/Headers/ruby
	$(LN) -vfsh . $(DSTROOT)/$(FW_VERSION_DIR)/Headers/ruby/ruby # support include "ruby/foo.h" from inside Headers/ruby
	$(LN) -vfsh Versions/Current/Ruby $(DSTROOT)$(FW_DIR)/Ruby
	find $(DSTROOT)$(FW_VERSION_DIR)$(USRLIBDIR) -name '*.a' -delete
	rsync -irptgoD --include='*/' --include='*.dylib' --include='*.bundle' --include='*.so' --include='ruby' --exclude='*' $(OBJROOT)/ $(SYMROOT)/
	find $(SYMROOT) -type f -perm -a+x | xargs -t -n 1 dsymutil
	find $(SYMROOT) -empty -delete
	find $(DSTROOT)$(FW_VERSION_DIR) -type f \( -name '*.so' -or -name '*.bundle' -or -name '*.dylib' \) | xargs -t $(STRIP) -x
	$(ECHO) Ignore signature warning, binary will be resigned
	$(STRIP) -x $(DSTROOT)$(FW_VERSION_DIR)$(USRBINDIR)/ruby
	codesign -f -s - $(DSTROOT)$(FW_VERSION_DIR)$(USRBINDIR)/ruby
	$(INSTALL_DIRECTORY) $(DSTROOT)$(USRBINDIR)
	for i in $(shell find "$(DSTROOT)$(FW_VERSION_DIR)$(USRBINDIR)" -type f); do \
		$(INSTALL_SCRIPT) $$i "$(DSTROOT)$(USRBINDIR)" || exit 1; \
	done
	$(INSTALL_DIRECTORY) "$(DSTROOT)/$(USRLIBDIR)"
	rsync -aim --include='*/' --include='*.dylib' --exclude='*' "$(DSTROOT)$(FW_VERSION_DIR)$(USRLIBDIR)/" "$(DSTROOT)$(USRLIBDIR)/"
	$(LN) -vfsh ../../System/Library/Frameworks/Ruby.framework/Versions/Current/usr/lib/ruby "$(DSTROOT)$(USRLIBDIR)"
	$(LN) -vfsh ../../System/Library/Frameworks/Ruby.framework/Versions/Current/usr/share/ri "$(DSTROOT)/usr/share/ri"
	$(INSTALL_DIRECTORY) "$(DSTROOT)/$(SITEDIR)"
	$(LN) -vfsh ../../../../../../../../..$(SITEDIR) "$(DSTROOT)/$(FW_VERSION_DIR)$(USRLIBDIR)/ruby/site_ruby"
	for i in $(shell find $(DSTROOT)$(FW_VERSION_DIR)$(USRLIBDIR) -type f -name 'libruby*dylib'); do \
		$(MV) $$i $(DSTROOT)/$(FW_VERSION_DIR)/Ruby || exit 1; \
	done
	(cd $(DSTROOT)$(FW_VERSION_DIR)$(USRLIBDIR); $(LN) -vsh ../../Ruby $$(readlink libruby.dylib))
	sh -x $(SRCROOT)/reexport.sh "$(DSTROOT)$(USRLIBDIR)/libruby.$(VERSION0).dylib" "$(DSTROOT)$(FW_VERSION_DIR)/Ruby" "${VERSION0}" "${VERSION}"
	# rdar://problem/8937160
	find $(DSTROOT) -type l -print0 | xargs -0 -t chmod -h go-w
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(MANDIR)/man1
	$(INSTALL_FILE) $(SRCROOT)/gem.1 $(DSTROOT)$(MANDIR)/man1
	# nuke duplicates that are only different in case
	find $(DSTROOT) -type f | sort -fr | uniq -id | xargs -t rm
	# Now deal with dirs
	find $(DSTROOT) -type d -path '*/ri/*' -empty -ls -delete
	@echo 'vvv case insensitive duplicates vvv'
	find $(DSTROOT) -print | sort -fr | uniq -id
	@echo '^^^ Case Insensitive Duplicates ^^^'
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(USRGEMDIR)
	find "$(OBJROOT)" -name "*.log" -print0 | while IFS= read -r -d $$'\0' mkmflog; do echo "Printing $$mkmflog"; cat "$$mkmflog"; done
	darwinvers=`$(SRCROOT)/ruby/tool/config.guess | sed -e 's/.*-//' | sed -e 's/\..*//'`; if [ ! -e "$(DSTROOT)/$(FW_VERSION_DIR)/$(USRLIBDIR)/ruby/$(VERSION0)/universal-$${darwinvers}/socket.bundle" ]; then exit 1; fi


OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(INSTALL_DIRECTORY) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL_DIRECTORY) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

ETC_DIR = $(DSTROOT)/private/etc

#need rails.1 rdoc.1 testrb.1
install-rails-placeholder:
	$(INSTALL_FILE) -m 555 $(EXTRAS_DIR)/rails $(DSTROOT)/usr/bin

install-irbrc:
	$(INSTALL_DIRECTORY) $(ETC_DIR)
	$(INSTALL_FILE) $(EXTRAS_DIR)/irbrc $(ETC_DIR)/irbrc

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(RM) $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	# Annotate the bison generated files
	for f in $(SRCROOT)/$(Project)/parse.{c,h} $(SRCROOT)/$(Project)/ext/ripper/ripper.c; do \
		ruby -i -pe '$$_ = $$_ + %(\n/* Apple Note: For the avoidance of doubt, Apple elects to distribute this file under the terms of the BSD license. */\n) if $$_ =~ /version 2.2 of Bison/' $$f && echo "Added BSD exception to $$f"; \
	done
	$(CP) $(SRCROOT)/extras/md5cc.{c,h} $(SRCROOT)/$(Project)/ext/digest/md5
	$(CP) $(SRCROOT)/extras/sha1cc.{c,h} $(SRCROOT)/$(Project)/ext/digest/sha1
	for patchfile in $(AEP_Patches); do \
		patch --verbose -d $(SRCROOT)/$(Project) -p0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
	$(TOUCH) $(SRCROOT)/$(Project)/ext/win32ole/.document
	$(RM) $(SRCROOT)/$(Project)/known_errors.inc
	$(CP) $(SRCROOT)/known_errors.def $(SRCROOT)/$(Project)/defs/known_errors.def
