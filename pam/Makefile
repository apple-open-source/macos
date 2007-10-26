##
# Makefile for Linux-PAM
##
#
# Luke Howard <lukeh@padl.com>
##

# Project info
Project               = pam
ProjectName           = PAM
UserType              = Administrator
ToolType              = Libraries
#GnuNoConfigure        = YES
GnuAfterInstall       = InstallModules InstallPamConf install-strip

# Uncomment for makefile debugging
RC_JASPER             = YES

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target        = install

Extra_CC_Flags = -Wall -DAR="$(SRCROOT)/ar.sh"
#Extra_CC_Flags        = -Wall -Ddarwin -traditional-cpp \
#			-I/System/Library/Frameworks/Security.framework/PrivateHeaders \
#			-I./include -I$(OBJROOT)/libpam/include -I$(OBJROOT) -I$(OBJROOT)/libpamc/include 

Install_Flags = 

Extra_Configure_Flags = --enable-fakeroot="$(DSTROOT)" \
			--enable-read-both-confs \
			--enable-sconfigdir=/private/etc/pam \
			--enable-securedir=/usr/lib/pam \
			--enable-giant-libpam \
			--disable-libcrack

build:: lazy_install_source configure

installhdrs::
	mkdir -p $(DSTROOT)/usr/include/pam
	$(INSTALL) -c -m 444 $(SRCROOT)/pam/_pam_aconf.h $(SRCROOT)/pam/libpam/include/pam/_pam_compat.h $(SRCROOT)/pam/libpam/include/pam/_pam_macros.h $(SRCROOT)/pam/libpam/include/pam/_pam_types.h $(SRCROOT)/pam/libpam/include/pam/pam_appl.h $(SRCROOT)/pam/libpamc/include/pam/pam_client.h $(SRCROOT)/pam/libpam_misc/include/pam/pam_misc.h $(SRCROOT)/pam/libpam/include/pam/pam_mod_misc.h $(SRCROOT)/pam/libpam/include/pam/pam_modules.h $(DSTROOT)/usr/include/pam
	

# Shadow the source tree
lazy_install_source:: shadow_source 
	@echo "Configuring $(Project)..."

#	$(_v) $(MKDIR) $(BuildDirectory)
#        $(_v) cd $(BuildDirectory) && ln -sf defs/darwin.defs default.defs

InstallModules:
#	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/System/Library/Security
#	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRLIBDIR)
#	$(_v) ( cd "$(DSTROOT)$(USRLIBDIR)" && ln -sf ../../System/Library/Security security )

InstallPamConf:
	$(_v) $(MKDIR) $(DSTROOT)/private/etc/pam.d
	-$(_v) for conf in `ls -1 $(SRCROOT)/pam.d`; do \
		$(INSTALL) -c -m $(Install_File_Mode) \
		$(SRCROOT)/pam.d/$${conf} $(DSTROOT)/private/etc/pam.d/$${conf}\
		; done

install-strip: installhdrs
	rm -f $(DSTROOT)/usr/lib/libpam.dylib \
		$(DSTROOT)/usr/lib/libpam_misc.dylib \
		$(DSTROOT)/usr/lib/libpamc.dylib
	$(_v) $(STRIP) -x $(DSTROOT)/usr/lib/libpam*.dylib
	mv $(DSTROOT)/usr/lib/libpam.1.0.dylib $(DSTROOT)/usr/lib/libpam.1.dylib
	ln -s libpam.1.dylib $(DSTROOT)/usr/lib/libpam.dylib
	ln -s libpam.1.dylib $(DSTROOT)/usr/lib/libpam_misc.dylib
	ln -s libpam.1.dylib $(DSTROOT)/usr/lib/libpamc.dylib
	ln -s libpam.1.dylib $(DSTROOT)/usr/lib/libpamc.1.dylib
	ln -s libpam.1.dylib $(DSTROOT)/usr/lib/libpam_misc.1.dylib
	ln -s libpam.1.dylib $(DSTROOT)/usr/lib/libpam.1.0.dylib
	$(_v) $(STRIP) -x $(DSTROOT)/usr/lib/pam/*.so
