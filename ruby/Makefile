##
# Makefile for ruby
##

Project                = ruby
Extra_CC_Flags         = -fno-common
GnuAfterInstall        = post-install install-plist
Extra_Configure_Flags  = --enable-pthread --enable-shared

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target         = install install-doc
Install_Flags          = DESTDIR=$(DSTROOT)

MAJOR     = 1
MINOR     = 8
VERSION   = $(MAJOR).$(MINOR)
SYSSTRING = `uname -p`-darwin`uname -r | cut -d. -f1-2`

post-install:
	$(STRIP) -x $(DSTROOT)/usr/bin/ruby
	$(STRIP) -x $(DSTROOT)/usr/lib/ruby/$(VERSION)/$(SYSSTRING)/*.bundle
	$(STRIP) -x $(DSTROOT)/usr/lib/ruby/$(VERSION)/$(SYSSTRING)/*/*.bundle
	$(RM) $(DSTROOT)/usr/lib/libruby-static.a
	$(STRIP) -x $(DSTROOT)/usr/lib/libruby.$(MAJOR).dylib
	ed - $(DSTROOT)/usr/lib/ruby/$(VERSION)/$(SYSSTRING)/rbconfig.rb < $(SRCROOT)/patches/fix_rbconfig.ed

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.8.2
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-configure \
                 patch-ext__dbm__extconf.rb \
                 patch-ext__digest__md5__extconf.rb \
                 patch-ext__digest__rmd160__extconf.rb \
                 patch-ext__digest__sha1__extconf.rb \
                 patch-ext__digest__sha2__extconf.rb \
                 patch-ext__dl__extconf.rb \
                 patch-ext__readline__extconf.rb \
                 patch-ext__socket__extconf.rb \
                 patch-ext__socket__socket.c \
                 patch-ext__zlib__extconf.rb \
                 patch-lib__mkmf.rb \
                 PR3917782.diff \
                 CAN-2005-2337.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
	cd $(SRCROOT)/$(Project) && patch -p1 < $(SRCROOT)/patches/ruby-1.8.2-xmlrpc-ipimethods-fix.diff
endif
