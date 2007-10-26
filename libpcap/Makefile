##
# Makefile for libpcap
##

# Project info
Project         = libpcap
UserType        = Developer
ToolType        = Libraries
GnuAfterInstall = shlibs install-shlibs install-plist make-manpages

# It's a GNU Source project
Install_Prefix = /usr
Install_Man = /usr/share/man
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
Extra_Configure_Flags = --enable-ipv6
Extra_CC_Flags = -I. -dynamic -fno-common -DHAVE_CONFIG_H -D_U_=\"\"

# Automatic Extract & Patch
AEP	       = YES
AEP_Project    = $(Project)
AEP_Version    = 0.9.5
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = wlt.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

extra_man_pages = breakloop close compile datalink datalink_name_to_val datalink_val_to_description \
	datalink_val_to_name dispatch dump dump_close dump_file dump_flush dump_fopen dump_ftell \
	dump_open file fileno findalldevs fopen_offline freealldevs freecode get_selectable_fd \
	geterr getnonblock inject is_swapped lib_version list_datalinks lookupdev lookupnet \
	loop major_version minor_version next next_ex open_dead open_live open_offline perror \
	sendpacket set_datalink setdirection setfilter setnonblock snapshot stats strerror

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif

lazy_install_source:: shadow_source
	@echo "*This needs to be installed from a case sensitive filesystem*"

Install_Target = install
installhdrs:: 
	$(MKDIR) -p $(DSTROOT)/usr/include/net
	$(INSTALL) -c -m 444 $(SRCROOT)/libpcap/pcap.h $(DSTROOT)/usr/include/
	$(INSTALL) -c -m 444 $(SRCROOT)/libpcap/pcap-namedb.h $(DSTROOT)/usr/include/

shlibs: 
	$(CC) $(CFLAGS) $(LDFLAGS) -dynamiclib -compatibility_version 1 -current_version 1 -all_load -install_name /usr/lib/libpcap.A.dylib -o $(OBJROOT)/libpcap.A.dylib $(OBJROOT)/libpcap.a
	$(RM) $(DSTROOT)/usr/include/net/bpf.h
	$(RM) $(DSTROOT)/usr/lib/libpcap.a
	$(RMDIR) $(DSTROOT)/usr/include/net

install-shlibs: 
	$(MKDIR) -p $(DSTROOT)/$(USRLIBDIR)
	$(INSTALL) -c $(OBJROOT)/libpcap.A.dylib $(DSTROOT)/$(USRLIBDIR)/
	$(STRIP) -S $(DSTROOT)/$(USRLIBDIR)/libpcap.A.dylib
	$(LN) -sf libpcap.A.dylib $(DSTROOT)/$(USRLIBDIR)/libpcap.dylib

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(OSV)/$(ProjectName).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName)/LICENSE $(OSL)/$(ProjectName).txt

make-manpages:
	for name in $(extra_man_pages); do \
		cd $(DSTROOT)/$(Install_Man)/man3 && echo ".so man3/pcap.3.gz" > pcap_$$name.3; \
	done
