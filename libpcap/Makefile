##
# Makefile for libpcap
##

# Project info
Project         = libpcap
UserType        = Developer
ToolType        = Libraries
GnuAfterInstall = shlibs install-shlibs

# It's a GNU Source project
Install_Prefix = /usr
Install_Man = /usr/share/man
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
Extra_Configure_Flags = --enable-ipv6
Extra_CC_Flags = -I. -dynamic -fno-common -DHAVE_CONFIG_H -D_U_=\"\"

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
