##
# Makefile for RubyGems 
##

Project = rubygems

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

RUBYUSR = $(DSTROOT)/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr

build::
	(cd $(SRCROOT)/$(Project) && RUBYUSR=$(RUBYUSR) ruby setup.rb all --prefix=$(RUBYUSR) --siterubyver=$(RUBYUSR)/lib/ruby/1.8)
	ruby -e "File.open(ARGV[0], 'w') { |io| io.write(Marshal.dump({})) }" $(RUBYUSR)/lib/ruby/gems/1.8/source_cache
	$(MKDIR) $(DSTROOT)/usr/bin
	for i in gem gem_server index_gem_repository.rb gem_mirror gemwhich update_rubygems gemlock gemri; do \
		$(LN) -fs ../../System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/bin/$$i $(DSTROOT)/usr/bin; \
	done
	$(MKDIR) $(DSTROOT)/Library/Ruby/Gems/1.8
	(cd $(DSTROOT)/Library/Ruby/Gems/1.8 && $(MKDIR) cache doc gems specifications && $(CP) $(RUBYUSR)/lib/ruby/gems/1.8/source_cache .)
	$(MKDIR) $(RUBYUSR)/lib/ruby
	$(LN) -fsh ../../../../../../../../../../Library/Ruby/Gems $(RUBYUSR)/lib/ruby/user-gems
	$(MKDIR) $(DSTROOT)/$(MANDIR)/man1
	$(INSTALL_FILE) $(SRCROOT)/gem.1 $(DSTROOT)$(MANDIR)/man1
	$(LN) $(DSTROOT)$(MANDIR)/man1/gem.1 $(DSTROOT)$(MANDIR)/man1/gem_mirror.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/gem.1 $(DSTROOT)$(MANDIR)/man1/gem_server.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/gem.1 $(DSTROOT)$(MANDIR)/man1/gemwhich.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/gem.1 $(DSTROOT)$(MANDIR)/man1/index_gem_repository.rb.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/gem.1 $(DSTROOT)$(MANDIR)/man1/update_rubygems.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/gem.1 $(DSTROOT)$(MANDIR)/man1/gemri.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/gem.1 $(DSTROOT)$(MANDIR)/man1/gemlock.1
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/COPYING $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 0.9.4
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tgz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = post_install.rb.diff \
                 lib_rubygems.rb.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
