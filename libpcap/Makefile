Project         = libpcap
ProjectVersion  = 1.0.0
Patches         = Makefile.in.diff configure.diff pcap-config.in.diff pcap-bpf.c.diff

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

install_headers::
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/include/pcap
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/pcap-bpf.h $(DSTROOT)/usr/include
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/pcap-namedb.h $(DSTROOT)/usr/include
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/pcap.h $(DSTROOT)/usr/include
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/pcap/bpf.h $(DSTROOT)/usr/include/pcap
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/pcap/namedb.h $(DSTROOT)/usr/include/pcap
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/pcap/pcap.h $(DSTROOT)/usr/include/pcap
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/pcap/sll.h $(DSTROOT)/usr/include/pcap
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/pcap/usb.h $(DSTROOT)/usr/include/pcap

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install::
	cd $(OBJROOT) && CFLAGS="$(CFLAGS)" $(SRCROOT)/$(Project)/configure --prefix=/usr --enable-ipv6

	$(MAKE) -C $(OBJROOT)
	$(CC) $(LDFLAGS) -dynamiclib -compatibility_version 1 -current_version 1 -all_load -install_name /usr/lib/libpcap.A.dylib -o $(OBJROOT)/libpcap.A.dylib $(OBJROOT)/libpcap.a

	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT)
	$(INSTALL_DYLIB) $(OBJROOT)/libpcap.A.dylib $(DSTROOT)/usr/lib
	$(LN) -s libpcap.A.dylib $(DSTROOT)/usr/lib/libpcap.dylib

	$(RM) $(DSTROOT)/usr/lib/libpcap.a

	$(CP) $(DSTROOT)/usr/lib/libpcap.A.dylib $(SYMROOT)
	$(STRIP) -S $(DSTROOT)/usr/lib/libpcap.A.dylib

	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(OSV)/$(ProjectName).plist
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName)/LICENSE $(OSL)/$(ProjectName).txt

	@$(MAKE) compress_man_pages
