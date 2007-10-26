##
# Makefile for ruby_dnssd
##

Project = ruby_dnssd

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

DSTRUBYDIR = $(DSTROOT)/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/lib/ruby
DSTGEMSDIR = $(DSTRUBYDIR)/gems/1.8

GEM = /usr/bin/gem
GEM_INSTALL = $(GEM) install --install-dir $(DSTGEMSDIR) --local --include-dependencies --rdoc

GEMS = dnssd

GEM_FILES_TO_CLEAN = ext

build::
	$(MKDIR) $(DSTGEMSDIR)
	(cd gems && $(GEM_INSTALL) $(GEMS)) 
	ditto $(DSTROOT) $(SYMROOT)
	strip -x `find $(DSTROOT) -name "*.bundle"`
	rm -rf `find $(DSTROOT) -name "*.dSYM" -type d`
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).txt $(OSL)/$(Project).txt
	(cd $(DSTGEMSDIR)/gems/dnssd*/ && rm -rf $(GEM_FILES_TO_CLEAN))
