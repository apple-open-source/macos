##
# Makefile for RubyCocoa
##

Project = RubyCocoa
ProjectVersion = trunk
FullProjectVersion = $(ProjectVersion)

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

RUBYUSR = $(DSTROOT)/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr
DSTRUBYDIR = $(RUBYUSR)/lib/ruby
DSTGEMSDIR = $(DSTRUBYDIR)/gems/1.8

GEM = /usr/bin/gem
GEM_INSTALL = $(GEM) install --install-dir $(DSTGEMSDIR) --local --include-dependencies --rdoc

DEV_USR_BIN = $(DSTROOT)/Developer/usr/bin
SAMPLES = $(DSTROOT)/Developer/Examples/Ruby/RubyCocoa

RUN_TESTS = 0

build::
	(cd $(SRCROOT)/$(Project) \
	&& /usr/bin/ruby install.rb config --prefix=$(RUBYUSR) --site-ruby=$(DSTRUBYDIR)/1.8 --so-dir=$(DSTRUBYDIR)/1.8/universal-darwin`uname -r | cut -d. -f1-2` --frameworks="$(DSTROOT)/System/Library/Frameworks" --xcode-extras="$(DSTROOT)/unmaintained_templates" --examples="$(DSTROOT)/Developer/Examples/Ruby" --documentation="$(DSTROOT)/Developer/Documentation" --gen-bridge-support=false --build-as-embeddable=false \
	&& /usr/bin/ruby install.rb setup \
	&& (if [ $(RUN_TESTS)"" = "1" ]; then (cd tests && (DYLD_FRAMEWORK_PATH=../framework/build /usr/bin/ruby -I../lib -I../ext/rubycocoa testall.rb || exit 1)); fi) \
	&& /usr/bin/ruby install.rb install \
	&& /usr/bin/ruby install.rb clean)
	rm -rf $(DSTROOT)/unmaintained_templates
	rm -rf $(DSTROOT)/Developer
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
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
