##
# Makefile for OpenPAM
##

export DESTDIR := $(DSTROOT)

# Project info
Project               = openpam
ProjectName           = OpenPAM
UserType              = Administrator
ToolType              = Libraries
GnuAfterInstall       = install-pam-shim relocate-sym-files install-pam-conf install-lib-fixup install-strip install-open-source-info

# Uncomment for makefile debugging
RC_JASPER             = YES

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target        = install

Extra_CC_Flags        = -Wall -DAR="$(SRCROOT)/ar.sh"
Install_Flags         = 
Extra_Configure_Flags = --with-modules-dir="/usr/lib/pam/" --x-lib=/usr/lib/libpam.2.dylib #--enable-debug

build:: lazy_install_source configure

# Shadow the source tree
lazy_install_source:: shadow_source 
	@echo "Configuring $(Project)..."


# Linux-PAM compatibility shim
VPATH := compat

install-pam-shim: $(OBJROOT)/libpam.1.dylib  # using GnuAfterInstall: rather than install:: so that relocating syms and stripping happens after building our shim
	install -o root -m 755 $^ $(DSTROOT)/usr/lib/

$(OBJROOT)/libpam.1.dylib: $(OBJROOT)/pam_shim.o
	$(CC) $(RC_CFLAGS) -g -install_name /usr/lib/libpam.1.dylib -dynamiclib -current_version 1.0.0 -compatibility_version 1.0.0 -Wl,-allowable_client,'!' $^ -o $@

$(OBJROOT)/pam_shim.o: pam_shim.c pam_shim_authenticate_flags.h pam_shim_chauthtok_flags.h pam_shim_flags.h pam_shim_item_type.h pam_shim_retval.h pam_shim_session_flags.h pam_shim_setcred_flags.h
	$(CC) -c -Wall $(RC_CFLAGS) -Os -g -o $@ $<


relocate-sym-files:
	$(CP) $(OBJROOT)/lib/.libs/libpam.2.0.0.dylib $(SYMROOT)/libpam.2.dylib
	$(CP) $(OBJROOT)/modules/pam_permit/.libs/pam_permit.2.0.0.so $(SYMROOT)/pam_permit.2.so
	$(CP) $(OBJROOT)/modules/pam_deny/.libs/pam_deny.2.0.0.so $(SYMROOT)/pam_deny.2.so
	$(CP) $(OBJROOT)/libpam.1.dylib $(SYMROOT)/libpam.1.dylib

install-pam-conf:
	$(_v) $(MKDIR) $(DSTROOT)/private/etc/pam.d
	$(_v) $(INSTALL) -c -m $(Install_File_Mode) $(SRCROOT)/pam.d/other $(DSTROOT)/private/etc/pam.d/other

install-lib-fixup:
	rm -f $(DSTROOT)/usr/lib/libpam.la \
		$(DSTROOT)/usr/lib/libpam.2.dylib \
		$(DSTROOT)/usr/lib/libpam.dylib
	mv $(DSTROOT)/usr/lib/libpam.2.0.0.dylib $(DSTROOT)/usr/lib/libpam.2.dylib
	ln -s libpam.2.dylib $(DSTROOT)/usr/lib/libpam.dylib
	rm -f $(DSTROOT)/usr/lib/pam/pam_deny.?.so \
		$(DSTROOT)/usr/lib/pam/pam_deny.la \
		$(DSTROOT)/usr/lib/pam/pam_deny.so \
		$(DSTROOT)/usr/lib/pam/pam_permit.?.so \
		$(DSTROOT)/usr/lib/pam/pam_permit.la \
		$(DSTROOT)/usr/lib/pam/pam_permit.so
	mv $(DSTROOT)/usr/lib/pam/pam_permit*.so $(DSTROOT)/usr/lib/pam/pam_permit.so.2
	mv $(DSTROOT)/usr/lib/pam/pam_deny*.so $(DSTROOT)/usr/lib/pam/pam_deny.so.2

install-strip: installhdrs
	$(_v) $(STRIP) -x $(DSTROOT)/usr/lib/libpam*.dylib
	$(_v) $(STRIP) -x $(DSTROOT)/usr/lib/pam/*.so.?

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-open-source-info:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(OSV)/$(ProjectName).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(ProjectName).txt
