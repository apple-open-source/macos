##
# Makefile for file
##

# Project info
Project               = file
ProjectVersion        = 5.00
Extra_Configure_Flags = --enable-fsect-man5 --disable-shared
Extra_CC_Flags        = -DBUILTIN_MACHO
GnuAfterInstall       = remove-libs install-plist strip-binary install-magic

Patches        = configure.diff \
                 magic__Magdir__archive.diff \
                 magic__Magdir__gnu.diff \
                 magic__Magdir__mach.diff \
                 magic__Magdir__sun.diff \
                 Mach-O.diff \
                 conformance.diff \
                 PR3881173.diff \
                 PR4324767.diff \
                 buildfix.diff \
                 PR6431343.diff \
                 strndup.diff \
                 cdf.c.diff

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -xf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.gz
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(Patches); do \
		patch -p0 -F0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done

remove-libs:
	$(MKDIR) $(DSTROOT)/usr/local/include
	$(MV) $(DSTROOT)/usr/include/magic.h $(DSTROOT)/usr/local/include
	$(RMDIR) $(DSTROOT)/usr/include
	$(MKDIR) $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libmagic.a $(DSTROOT)/usr/local/lib
	$(RMDIR) $(DSTROOT)/usr/lib
	$(RMDIR) $(DSTROOT)/usr/share/man/man3
	$(RMDIR) $(DSTROOT)/usr/share/man/man4

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

strip-binary:
	$(MKDIR) $(SYMROOT)/usr/bin
	$(CP) $(DSTROOT)/usr/bin/file $(SYMROOT)/usr/bin
	$(STRIP) $(DSTROOT)/usr/bin/file

install-magic:
	$(MKDIR) $(DSTROOT)/usr/share/file/magic
	$(INSTALL_FILE) $(Sources)/magic/Magdir/* $(DSTROOT)/usr/share/file/magic
