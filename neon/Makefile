Project               = neon
UserType              = Administrator
ToolType              = Commands
Extra_Configure_Flags = --enable-shared --enable-static \
                        --with-expat --with-ssl \
                        --bindir=/usr/local/bin \
                        --mandir=/usr/local/share/man \
                        --datadir=/usr/local/share \
                        --includedir=/usr/local/include
GnuNoBuild            = YES
GnuNoInstall          = YES
GnuAfterInstall       = install-plist post-install

# Hack!
build:: configure
	ed - $(BuildDirectory)/config.h < $(SRCROOT)/files/fix_config.h.ed
	$(_v) $(MAKE) -C $(BuildDirectory)
install:: build
	$(_v) $(MAKE) -C $(BuildDirectory) \
		DESTDIR=$(DSTROOT) pkgconfigdir=/usr/local/lib/pkgconfig \
		install

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

post-install:
	ed - $(DSTROOT)/usr/local/bin/neon-config < $(SRCROOT)/files/remove_arch_flags.ed
	ed - $(DSTROOT)/usr/local/lib/pkgconfig/neon.pc < $(SRCROOT)/files/remove_arch_flags.ed
	$(CP) $(DSTROOT)/usr/lib/libneon.26.0.3.dylib $(SYMROOT)
	$(STRIP) -x $(DSTROOT)/usr/lib/libneon.26.0.3.dylib
	$(MKDIR) $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libneon.a $(DSTROOT)/usr/local/lib

# Automatic Extract & Patch
AEP_Project    = neon
AEP_Version    = 0.26.3
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = configure.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/files/$$patchfile || exit 1; \
	done

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/src/COPYING.LIB $(OSL)/$(Project).txt
