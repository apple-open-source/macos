Project         = tcpdump
ProjectVersion  = 4.0.0
Patches         = PR-6152397.diff configure.diff PR-6477262.diff

Extra_CC_Flags = -mdynamic-no-pic

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjVersion)
	$(TAR) -C $(SRCROOT) -xf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.gz
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for file in $(Patches); do \
		patch -p0 -F0 -i $(SRCROOT)/patches/$$file; \
	done


OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install::
	cd $(OBJROOT) && CFLAGS="$(CFLAGS)" $(SRCROOT)/$(Project)/configure --prefix=/usr --enable-ipv6

	$(MAKE) -C $(OBJROOT)

	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT)

	$(RM) $(DSTROOT)/usr/sbin/tcpdump.$(ProjectVersion)

	$(CP) $(DSTROOT)/usr/sbin/tcpdump $(SYMROOT)
	$(STRIP) $(DSTROOT)/usr/sbin/tcpdump

	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(OSV)/$(ProjectName).plist
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName)/LICENSE $(OSL)/$(ProjectName).txt

	@$(MAKE) compress_man_pages
