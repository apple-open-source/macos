##
# Makefile for fcgi 
##

# Project info
Project               = fcgi
Extra_Configure_Flags = --prefix=/usr --disable-static
GnuAfterInstall       = clean-dstroot install-ruby-binding install-plist-licenses

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags   = DESTDIR=$(DSTROOT)

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

clean-dstroot:
	$(RM) $(DSTROOT)/usr/lib/libfcgi*.la
	$(STRIP) -x $(DSTROOT)/usr/lib/*.dylib

install-plist-licenses:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/LICENSE.TERMS $(OSL)/$(Project).txt
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/ruby-fcgi.COPYING $(OSL)/$(Project)-ruby.txt

DSTRUBYDIR = $(DSTROOT)/System/Library/Frameworks/Ruby.framework/Versions/1.8/usr/lib/ruby
DSTGEMSDIR = $(DSTRUBYDIR)/gems/1.8

GEM = /usr/bin/gem
GEM_INSTALL = $(GEM) install --install-dir $(DSTGEMSDIR) --local --include-dependencies --rdoc

GEMS = fcgi

GEM_FILES_TO_CLEAN = ChangeLog README README.signals ext

install-ruby-binding:
	$(MKDIR) $(DSTGEMSDIR)
	(cd gems && $(GEM_INSTALL) $(GEMS) -- with-fcgi-dir=$(DSTROOT)/usr)
	ditto $(DSTROOT) $(SYMROOT)
	strip -x `find $(DSTROOT) -name "*.bundle"`
	rm -rf `find $(DSTROOT) -name "*.dSYM" -type d`
	(cd $(DSTGEMSDIR)/gems/fcgi*/ && rm -rf $(GEM_FILES_TO_CLEAN))

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 2.4.0
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = configure.diff fcgi-2.4.0-Makefile.in.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
