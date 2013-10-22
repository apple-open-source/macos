##
# Makefile for RubyCocoa
##

Project = RubyCocoa
ProjectVersion = trunk
FullProjectVersion = $(ProjectVersion)

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

RUBYHOSTUSR = /System/Library/Frameworks/Ruby.framework/Versions/1.8/usr
RUBYUSR = $(DSTROOT)/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr
DSTRUBYDIR = $(RUBYUSR)/lib/ruby

RUN_TESTS = 0

build::
	(cd $(SRCROOT)/$(Project) \
	&& $(RUBYHOSTUSR)/bin/ruby install.rb config --sdkroot=$(SDKROOT) --prefix=$(RUBYUSR) --site-ruby=$(DSTRUBYDIR)/1.8 --so-dir=$(DSTRUBYDIR)/1.8/universal-darwin`uname -r | cut -d. -f1-2` --frameworks="$(DSTROOT)/System/Library/Frameworks" --xcode-extras="$(DSTROOT)/unmaintained_templates" --examples="$(DSTROOT)/Developer/Examples/Ruby" --documentation="$(DSTROOT)/Developer/Documentation" --gen-bridge-support=false --build-as-embeddable=false \
	&& $(RUBYHOSTUSR)/bin/ruby install.rb setup \
	&& (if [ $(RUN_TESTS)"" = "1" ]; then (cd tests && (DYLD_FRAMEWORK_PATH=../framework/build /usr/bin/ruby -I../lib -I../ext/rubycocoa testall.rb || exit 1)); fi) \
	&& $(RUBYHOSTUSR)/bin/ruby install.rb install)
	rm -rf $(DSTROOT)/unmaintained_templates
	rm -rf $(DSTROOT)/Developer
	find $(RUBYUSR)/lib -name "*.bundle" -exec cp '{}' $(SYMROOT) \;
	cp $(DSTROOT)/System/Library/Frameworks/RubyCocoa.framework/RubyCocoa $(SYMROOT)
	for f in $(SYMROOT)/*; do dsymutil $$f; done
	$(STRIP) -x `find $(RUBYUSR)/lib -name "*.bundle"` 
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/COPYING $(OSL)/$(Project).txt

installhdrs::
	@echo Nothing to be done for installhdrs

sync:
	sync_out="RubyCocoa-trunk"; \
	rm -rf $$sync_out; \
	svn co --ignore-externals -q https://rubycocoa.svn.sourceforge.net/svnroot/rubycocoa/trunk/src $$sync_out; \
	(find $$sync_out -name ".svn" -exec rm -rf {} \; >& /dev/null || true); \
	(cd $$sync_out && rm -f misc/bridge-support-tiger.tar.gz misc/libruby.1.dylib-tiger.tar.gz); \
	rm -f $$sync_out.tgz; \
	tar -czf $$sync_out.tgz $$sync_out; \
	rm -rf $$sync_out; \
	echo "done"

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = $(FullProjectVersion)
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tgz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = fix_install.rb.diff fix_ext_rubycocoa_extconf.rb.diff disable_threading_hacks.diff fix_metaconfig.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		echo $$patchfile; \
		patch -d $(SRCROOT)/$(Project) -p0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
	sed -i -e 's:-I/usr/include/:-I$(SDKROOT)/usr/include/:' $(SRCROOT)/$(Project)/pre-config.rb
