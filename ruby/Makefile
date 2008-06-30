##
# Makefile for ruby
##

FWS_DIR = $(DSTROOT)/System/Library/Frameworks
FW_DIR = $(FWS_DIR)/Ruby.framework
LOCAL_FW_RES_DIR = $(EXTRAS_DIR)/framework_resources
FW_VERSION_DIR = $(FW_DIR)/Versions/$(VERSION)
ULB = $(DSTROOT)/usr/local/bin
SITEDIR = $(DSTROOT)/Library/Ruby/Site

Project                = ruby
Extra_CC_Flags         = -fno-common -DENABLE_DTRACE
GnuNoBuild             = YES
GnuAfterInstall        = post-install install-manpage install-plist install-sample install-irbrc install-xcode-template install-dtrace-sample install-xray-template
Extra_Configure_Flags  = --enable-pthread --enable-shared --prefix=/System/Library/Frameworks/Ruby.framework/Versions/$(VERSION)/usr --with-sitedir=/Library/Ruby/Site

# [gs]etcontext() functions are broken
Extra_Configure_Flags += ac_cv_func_getcontext=no ac_cv_func_setcontext=no

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target         = install install-doc
Install_Flags          = DESTDIR=$(DSTROOT)

MAJOR     = 1
MINOR     = 8
VERSION   = $(MAJOR).$(MINOR)
SYSSTRING = universal-darwin`uname -r | cut -d. -f1-2`

EXTRAS_DIR = $(SRCROOT)/extras

# patch config.h
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)
	$(MKDIR) $(FW_VERSION_DIR)/Resources
	$(INSTALL_FILE) $(LOCAL_FW_RES_DIR)/Info.plist \
		$(FW_VERSION_DIR)/Resources/Info.plist
	$(INSTALL_FILE) $(LOCAL_FW_RES_DIR)/version.plist \
		$(FW_VERSION_DIR)/Resources/version.plist
	$(MKDIR) $(FW_VERSION_DIR)/Resources/English.lproj
	$(INSTALL_FILE) $(LOCAL_FW_RES_DIR)/English.lproj/InfoPlist.strings \
		$(FW_VERSION_DIR)/Resources/English.lproj/InfoPlist.strings
	$(LN) -fsh $(VERSION) $(FW_DIR)/Versions/Current 
	$(MKDIR) $(FW_VERSION_DIR)/Headers
	$(LN) -fsh Versions/Current/Headers $(FW_DIR)/Headers
	$(LN) -fsh Versions/Current/Ruby $(FW_DIR)/Ruby
	$(LN) -fsh Versions/Current/Resources $(FW_DIR)/Resources

build:: configure
	$(_v) $(MAKE) -C $(BuildDirectory)

$(ConfigStamp2): $(ConfigStamp)
	ed - $(OBJROOT)/config.h < $(SRCROOT)/patches/fix_config.ed
	sed -E -i '' 's/#define alloca alloca//' $(OBJROOT)/config.h
	$(TOUCH) $(ConfigStamp2)

post-install:
	mv $(FW_VERSION_DIR)/usr/share/ri/$(VERSION)/system/REXML/Parsers/XPathParser/Predicate-i.yaml $(FW_VERSION_DIR)/usr/share/ri/$(VERSION)/system/REXML/Parsers/XPathParser/predicate-i.yaml
	mv $(FW_VERSION_DIR)/usr/share/ri/$(VERSION)/system/Exception2MessageMapper/Fail-i.yaml $(FW_VERSION_DIR)/usr/share/ri/$(VERSION)/system/Exception2MessageMapper/fail-i.yaml
	ditto $(DSTROOT) $(SYMROOT)
	find $(SYMROOT) -type f -and -not \( -name "*.dylib" -or -name "*.bundle" -or -path "*/usr/bin/ruby" \) -delete	
	find $(SYMROOT) -type d -empty -delete
	$(STRIP) -x $(FW_VERSION_DIR)/usr/bin/ruby
	$(MKDIR) $(ULB)
	$(CP) $(FW_VERSION_DIR)/usr/bin/ruby $(ULB)/ruby64
	lipo -remove ppc64 -output $(FW_VERSION_DIR)/usr/bin/ruby $(FW_VERSION_DIR)/usr/bin/ruby
	lipo -remove x86_64 -output $(FW_VERSION_DIR)/usr/bin/ruby $(FW_VERSION_DIR)/usr/bin/ruby
	$(STRIP) -x $(FW_VERSION_DIR)/usr/lib/ruby/$(VERSION)/$(SYSSTRING)/*.bundle
	$(STRIP) -x $(FW_VERSION_DIR)/usr/lib/ruby/$(VERSION)/$(SYSSTRING)/*/*.bundle
	$(RM) $(FW_VERSION_DIR)/usr/lib/libruby-static.a
	$(STRIP) -x $(FW_VERSION_DIR)/usr/lib/libruby.$(MAJOR).dylib
	ed - $(FW_VERSION_DIR)/usr/lib/ruby/$(VERSION)/$(SYSSTRING)/rbconfig.rb < $(SRCROOT)/patches/fix_rbconfig.ed
	(cd $(FW_VERSION_DIR)/usr/lib/ruby/$(VERSION)/$(SYSSTRING) && sed -E -i '' "s/-arch (ppc|ppc64|i386|x86_64)//g" rbconfig.rb && patch -p0 < $(SRCROOT)/patches/rbconfig.diff)
	$(LN) -fsh usr/lib/libruby.dylib $(FW_VERSION_DIR)/Ruby
	(cd $(FW_VERSION_DIR) && for i in `find usr/lib/ruby/1.8/ -name "*.h"`; do $(LN) -fs ../$$i Headers/`basename $$i`; done)
	$(MKDIR) $(DSTROOT)/usr/bin
	for i in `find $(FW_VERSION_DIR)/usr/bin -type f`; do \
		$(LN) -fs ../../System/Library/Frameworks/Ruby.framework/Versions/Current/usr/bin/`basename $$i` $(DSTROOT)/usr/bin; \
	done
	$(MKDIR) $(DSTROOT)/usr/lib
	for i in `find $(FW_VERSION_DIR)/usr/lib -name "*.dylib"`; do \
		$(LN) -fs ../../System/Library/Frameworks/Ruby.framework/Versions/Current/usr/lib/`basename $$i` $(DSTROOT)/usr/lib; \
	done
	$(LN) -fsh ../../System/Library/Frameworks/Ruby.framework/Versions/Current/usr/lib/ruby $(DSTROOT)/usr/lib/ruby
	$(LN) -fsh ../../System/Library/Frameworks/Ruby.framework/Versions/Current/usr/share/ri $(DSTROOT)/usr/share/ri
	$(MKDIR) $(SITEDIR)
	$(LN) -fsh ../../../../../../../../../../Library/Ruby/Site $(FW_VERSION_DIR)/usr/lib/ruby/site_ruby
#	sh $(EXTRAS_DIR)/test_framework.sh $(FWS_DIR) || exit 1

install-manpage:
	$(INSTALL_FILE) $(SRCROOT)/irb.1 $(DSTROOT)$(MANDIR)/man1
	$(LN) $(DSTROOT)$(MANDIR)/man1/irb.1 $(DSTROOT)$(MANDIR)/man1/erb.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/irb.1 $(DSTROOT)$(MANDIR)/man1/ri.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/irb.1 $(DSTROOT)$(MANDIR)/man1/rdoc.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/irb.1 $(DSTROOT)$(MANDIR)/man1/testrb.1

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

SAMPLE_DIR = $(DSTROOT)/Developer/Examples/Ruby/Ruby

install-sample:
	$(MKDIR) $(SAMPLE_DIR)
	$(CP) $(Sources)/sample/* $(SAMPLE_DIR)
	find $(SAMPLE_DIR) -type f -print0 | xargs -0 sed -E -i '' "s/\#\! *\/usr\/local\/bin\/ruby/\#\!\/usr\/bin\/env ruby/g"

ETC_DIR = $(DSTROOT)/private/etc

install-irbrc:
	$(MKDIR) $(ETC_DIR)
	$(INSTALL_FILE) $(EXTRAS_DIR)/irbrc $(ETC_DIR)/irbrc

TEMPL_DIR = $(DSTROOT)/Developer/Library/Xcode/Project\ Templates

install-xcode-template:
	$(MKDIR) $(TEMPL_DIR)
	ditto $(EXTRAS_DIR)/xcode_template $(TEMPL_DIR)
	find $(TEMPL_DIR) -name ".svn" -print0 | xargs -0 rm -rf

DTRACE_SAMPLE = $(DSTROOT)/Developer/Examples/Ruby/DTrace

install-dtrace-sample:
	$(MKDIR) $(DTRACE_SAMPLE)
	$(CP) $(EXTRAS_DIR)/dtrace_sample/*.d $(DTRACE_SAMPLE)

XRAY_PLUGINS = $(DSTROOT)/Developer/Library/Instruments/PlugIns

install-xray-template:
	$(MKDIR) $(XRAY_PLUGINS)
	$(CP) $(EXTRAS_DIR)/ruby.usdt $(XRAY_PLUGINS)

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 1.8.6-p114
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-configure \
                 patch-lib__mkmf.rb \
                 PR3917782.diff \
                 PR4224980.diff \
                 PR3855181.diff \
                 ruby.c.diff \
                 PR4346845.diff \
                 ruby.1.diff \
                 process.c.diff \
                 ext_extmk.rb.diff \
                 ext_digest_md5_extconf.rb.diff \
                 ext_digest_md5_md5.h.diff \
                 ext_digest_sha1_extconf.rb.diff \
                 ext_digest_sha1_sha1.h.diff \
                 ext_tk_extconf.rb.diff \
                 lib_rdoc_usage.rb.diff \
                 lib_rdoc_ri_ri_options.rb.diff \
                 lib_rdoc_ri_ri_paths.rb.diff \
                 lib_irb_init.rb.diff \
                 dtrace.diff \
                 ruby_thread_hooks.diff \
                 rexml_bugs.diff \
                 5877968.diff \
                 5884052.diff \
                 5879206.diff \
                 5876803.diff \

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
	$(TOUCH) $(SRCROOT)/$(Project)/ext/win32ole/.document
	dtrace -h -s $(EXTRAS_DIR)/dtrace.d -o $(SRCROOT)/$(Project)/dtrace.h
