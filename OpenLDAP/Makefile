##
# Makefile for OpenLDAP
##

# Project info
Project               = OpenLDAP
ProjectName           = OpenLDAP
UserType              = Administrator
ToolType              = Commands

Extra_CC_Flags        = -I${SRCROOT}/OpenLDAP/include -I${OBJROOT}/include -I${SRCROOT}/OpenLDAP/libraries/libldap -I${SRCROOT}/OpenLDAP/servers/slapd -I/AppleInternal/Developer/Headers/sasl -fno-common
Extra_LD_Flags        = -L${OBJROOT}/libraries
Extra_Environment     = CPPFLAGS=-I/AppleInternal/Developer/Headers/sasl

GnuAfterInstall = apple_port

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Extra_Configure_Flags = --disable-shared --disable-bdb --enable-ldbm --x-includes=/AppleInternal/Developer/Headers
Extra_Configure_Flags += --prefix=${DSTROOT}/private --sysconfdir=${DSTROOT}/private/etc --localstatedir=${DSTROOT}/private/var/db/openldap

Install_Target = install

# NetInfo - LDAP Bridge
Extra_CC_Flags        += -F/System/Library/PrivateFrameworks
Extra_LD_Libraries    += -framework NetInfo
Extra_Configure_Flags += --enable-netinfo --enable-local --enable-crypt --with-tls --program-transform-name="s/^sl/ni-sl/"

apple_port:
	rm -rf ${OBJROOT}/libraries/liblber/*test.o
	rm -rf ${OBJROOT}/libraries/libldap/*test.o
	rm -rf ${OBJROOT}/libraries/libldap_r/*test.o
	libtool -dynamic -install_name /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP -compatibility_version 1.0.0 -current_version 2.0.0 -o ${SYMROOT}/LDAP ${OBJROOT}/libraries/liblber/*.o ${OBJROOT}/libraries/libldap_r/*.o -lsasl2 -lcrypto -lssl -lcc_dynamic -framework System
	mkdir -p ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Headers
	mkdir -p ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Resources
	ln -s A  ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/Current
	ln -s Versions/Current/Headers ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Headers
	ln -s Versions/Current/LDAP ${DSTROOT}/System/Library/Frameworks/LDAP.framework/LDAP
	ln -s Versions/Current/Resources ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Resources
	strip -S ${SYMROOT}/LDAP -o ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/LDAP
	cp ${DSTROOT}/usr/include/ldap.h ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Headers
	cp ${DSTROOT}/usr/include/lber.h ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Headers
	cp ${SRCROOT}/AppleExtras/Resources/Info.plist ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Resources
	cp ${SRCROOT}/AppleExtras/Resources/version.plist ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Resources
	rm -rf ${DSTROOT}/usr/lib
	mkdir -p ${DSTROOT}/usr/lib
	ln -s /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP ${DSTROOT}/usr/lib/liblber.dylib
	ln -s /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP ${DSTROOT}/usr/lib/libldap.dylib
	ln -s /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP ${DSTROOT}/usr/lib/libldap_r.dylib
