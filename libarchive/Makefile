Project        = libarchive
ProjectVersion = 2.6.2
Patches        = Makefile.in.diff copyfile.patch symlink-follow.diff bsdtar-notapedrive.diff cpio-I-option.diff
Extra_CC_Flags = -Wall

InstallPrefix    = /usr

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.gz
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for file in $(Patches); do \
		patch -p0 -F0 -i $(SRCROOT)/files/$$file || exit 1; \
	done

install::
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project)/configure --disable-dependency-tracking --prefix=$(InstallPrefix) --disable-static --enable-bsdcpio
	$(MAKE) -C $(OBJROOT)
	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT)

	@echo === post-install cleanup ===

	$(CP) $(DSTROOT)$(InstallPrefix)/bin/bsdtar $(SYMROOT)
	$(STRIP) -x $(DSTROOT)$(InstallPrefix)/bin/bsdtar

	$(MV) $(DSTROOT)$(InstallPrefix)/bin/bsdcpio $(DSTROOT)$(InstallPrefix)/bin/cpio
	$(MV) $(DSTROOT)$(InstallPrefix)/share/man/man1/bsdcpio.1 $(DSTROOT)$(InstallPrefix)/share/man/man1/cpio.1
	$(CP) $(DSTROOT)$(InstallPrefix)/bin/cpio $(SYMROOT)
	$(STRIP) -x $(DSTROOT)$(InstallPrefix)/bin/cpio

	$(CP) $(DSTROOT)$(InstallPrefix)/lib/libarchive.$(ProjectVersion).dylib $(SYMROOT)
	$(STRIP) -x $(DSTROOT)$(InstallPrefix)/lib/libarchive.$(ProjectVersion).dylib

	$(LN) -s bsdtar $(DSTROOT)$(InstallPrefix)/bin/tar
	$(LN) -s bsdtar.1 $(DSTROOT)$(InstallPrefix)/share/man/man1/tar.1

	$(MKDIR) $(DSTROOT)/usr/local
	$(MV) $(DSTROOT)$(InstallPrefix)/include $(DSTROOT)/usr/local

	$(RM) $(DSTROOT)$(InstallPrefix)/lib/libarchive.la
	$(RMDIR) $(DSTROOT)$(InstallPrefix)/share/man/man3
	$(RMDIR) $(DSTROOT)$(InstallPrefix)/share/man/man5

	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/COPYING $(OSL)/$(Project).txt

	@$(MAKE) compress_man_pages
