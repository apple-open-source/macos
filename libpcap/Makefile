##
# Makefile for libpcap
##

# Project info
Project         = libpcap
UserType        = Developer
ToolType        = Libraries
GnuAfterInstall = shlibs install-shlibs

# It's a GNU Source project
Install_Prefix = /usr/local
Install_Man = /usr/local/share/man
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Extra_CC_Flags = -I. -dynamic -fno-common

lazy_install_source:: shadow_source
	@echo "*This needs to be installed from a case sensitive filesystem*"

Install_Target = install
installhdrs:: install
shlibs: 
	$(CC) $(CFLAGS) $(LDFLAGS) -dynamiclib -compatibility_version 1 -current_version 1 -all_load -install_name /usr/lib/libpcap.A.dylib -o $(OBJROOT)/libpcap.A.dylib $(OBJROOT)/libpcap.a

install-shlibs: 
	$(MKDIR) -p $(DSTROOT)/$(USRLIBDIR)
	$(INSTALL) -c $(OBJROOT)/libpcap.A.dylib $(DSTROOT)/$(USRLIBDIR)/
	$(STRIP) -S $(DSTROOT)/$(USRLIBDIR)/libpcap.A.dylib
	$(LN) -sf libpcap.A.dylib $(DSTROOT)/$(USRLIBDIR)/libpcap.dylib
