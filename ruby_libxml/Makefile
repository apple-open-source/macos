##
# Makefile for ruby_libxml
##

Project = libxml-ruby

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

DSTRUBYDIR = $(DSTROOT)/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/lib/ruby
DSTGEMSDIR = $(DSTRUBYDIR)/gems/1.8

GEM = /usr/bin/gem
GEM_INSTALL = $(GEM) install --install-dir $(DSTGEMSDIR) --local --include-dependencies --rdoc

GEMS = libxml-ruby

GEM_FILES_TO_CLEAN = CHANGELOG LICENSE README Rakefile TODO tests ext

build::
	$(MKDIR) $(DSTGEMSDIR)
	(cd gems && $(GEM_INSTALL) $(GEMS)) 
	ditto $(DSTROOT) $(SYMROOT)
	strip -x `find $(DSTROOT) -name "*.bundle"`
	rm -rf `find $(DSTROOT) -name "*.dSYM" -type d`
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(DSTGEMSDIR)/gems/libxml-ruby*/LICENSE $(OSL)/$(Project).txt
	(cd $(DSTGEMSDIR)/gems/libxml-ruby*/ && rm -rf $(GEM_FILES_TO_CLEAN))
