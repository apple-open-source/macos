#
# XBS-compatible Makefile for automake.
#

Project        = automake
ProjectVersion = 1.10
Patches        = patch-6165353-Xcode.in

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -jxof $(SRCROOT)/$(Project)-$(ProjectVersion).tar.bz2
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for file in $(Patches); do \
		patch -p0 -i $(SRCROOT)/patches/$$file || exit 1; \
	done

install::
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project)/configure --prefix=/usr
	$(MAKE) -C $(OBJROOT)
	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT)
	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/COPYING $(OSL)/$(Project).txt
	$(MKDIR) $(DSTROOT)/usr/share/man/man1
	$(RM) $(DSTROOT)/usr/share/info/dir
	$(INSTALL_FILE) $(SRCROOT)/automake.1 $(DSTROOT)/usr/share/man/man1
	$(LN) -s automake.1 $(DSTROOT)/usr/share/man/man1/automake-1.10.1
	$(LN) -s automake.1 $(DSTROOT)/usr/share/man/man1/aclocal.1
	$(LN) -s automake.1 $(DSTROOT)/usr/share/man/man1/aclocal-1.10.1
	$(MAKE) compress_man_pages
