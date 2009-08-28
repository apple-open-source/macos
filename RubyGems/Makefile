##
# Makefile for RubyGems 
##

Project = rubygems

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

RUBYUSR = $(DSTROOT)/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr

build::
	(cd $(SRCROOT)/$(Project) && /usr/bin/ruby setup.rb --prefix=$(DSTROOT)/prefix --no-rdoc --no-ri)
	ditto $(DSTROOT)/prefix/bin $(RUBYUSR)/bin
	ditto $(DSTROOT)/prefix/lib $(RUBYUSR)/lib/ruby/1.8
	rm -rf $(DSTROOT)/prefix
	#/usr/bin/ruby -e "File.open(ARGV[0], 'w') { |io| io.write(Marshal.dump({})) }" $(RUBYUSR)/lib/ruby/gems/1.8/source_cache
	$(MKDIR) $(DSTROOT)/usr/bin
	$(LN) -fs ../../System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/bin/gem $(DSTROOT)/usr/bin; \
	$(MKDIR) $(RUBYUSR)/lib/ruby/gems/1.8
	(cd $(RUBYUSR)/lib/ruby/gems/1.8 && $(MKDIR) cache doc gems specifications)
	$(MKDIR) $(DSTROOT)/Library/Ruby/Gems/1.8
	(cd $(DSTROOT)/Library/Ruby/Gems/1.8 && $(MKDIR) cache doc gems specifications)
	$(MKDIR) $(RUBYUSR)/lib/ruby
	$(LN) -fsh ../../../../../../../../../../Library/Ruby/Gems $(RUBYUSR)/lib/ruby/user-gems
	$(MKDIR) $(DSTROOT)/$(MANDIR)/man1
	$(INSTALL_FILE) $(SRCROOT)/gem.1 $(DSTROOT)$(MANDIR)/man1
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/COPYING $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 1.3.1
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tgz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = 

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
