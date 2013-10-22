##
# Makefile for OpenLDAP
##

# Project info
Project               = OpenLDAP
ProjectName           = OpenLDAP
UserType              = Administrator
ToolType              = Commands

Extra_CC_Flags        = -DLDAP_RESPONSE_RB_TREE=1 -DLDAP_DEPRECATED=1 -DLDAP_CONNECTIONLESS=1 -DSLAP_DYNACL=1 -DUSES_KRBNAME=1 -DTGT_OS_VERSION="\\\"$(MACOSX_DEPLOYMENT_TARGET)\\\"" -DPROJVERSION="\\\"$(RC_ProjectSourceVersion)\\\"" -I/usr/local/BerkeleyDB/include -I/usr/include/krb5 -I${SRCROOT}/OpenLDAP/include -I${OBJROOT}/include -I${SRCROOT}/OpenLDAP/libraries/libldap -I${SRCROOT}/OpenLDAP/servers/slapd -I/usr/include/sasl -fno-common -Os -Wno-format-extra-args
Extra_LD_Flags        = -L${OBJROOT}/libraries -L/usr/local/BerkeleyDB/lib/
Extra_Environment     = CPPFLAGS="-I/usr/include/sasl -I/usr/local/BerkeleyDB/include"
Extra_Environment    += AR=${SRCROOT}/ar.sh

ifeq "$(DSS_BUILD_PARALLEL)" "yes"
ncpus = $(shell sysctl hw.ncpu | awk '{print $$2}')
Extra_Make_Flags    += -j$(ncpus)
endif

ifeq "$(RC_ProjectName)" "LDAPFramework"
GnuAfterInstall = apple_framework
else
GnuAfterInstall = apple_port
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Extra_Configure_Flags = --disable-shared --disable-cleartext --enable-bdb  --x-libraries=/usr/local/BerkeleyDB/lib
Extra_Configure_Flags += --prefix=${DSTROOT}/private --sysconfdir=${DSTROOT}/private/etc --localstatedir=${DSTROOT}/private/var/db/openldap --enable-aci=yes
#Extra_Configure_Flags +=  DESTDIR="${DSTROOT}" --bindir="$(BINDIR)" --sbindir="$(SBINDIR)" --libexecdir="/usr/libexec" --datadir="$(SHAREDIR)/openldap" --sysconfdir="$(ETCDIR)" --localstatedir="${VARDIR}/db/openldap"
Extra_Configure_Flags +=  --enable-overlays=yes --enable-dynid=yes --enable-auditlog=yes --enable-unique=yes --enable-odlocales=yes --enable-odusers=yes
Install_Target = install

Extra_CC_Flags        += -F/System/Library/PrivateFrameworks -F/System/Library/Frameworks/OpenDirectory.framework/Frameworks
Extra_LD_Libraries    += -framework CoreFoundation -framework Security -framework SystemConfiguration -framework IOKit 
ifeq "$(RC_ProjectName)" "LDAPFramework"
Extra_LD_Libraries    += 
else
Extra_LD_Libraries    += -framework Heimdal -framework OpenDirectory -framework HeimODAdmin -framework CommonAuth -lpac -framework PasswordServer
endif
Extra_Configure_Flags += --enable-local --enable-crypt --with-tls --program-transform-name="s/^sl/ni-sl/"

ifeq "$(RC_ProjectName)" "LDAPFramework"
Extra_Configure_Flags += --enable-slapd="no"
else
Extra_Configure_Flags += --enable-slapd="yes"
endif

ORDERFILE=${SRCROOT}/AppleExtras/LDAP.order
ifeq "$(shell test -f $(ORDERFILE) && echo YES )" "YES"
	LDAP_SECTORDER_FLAGS=-sectorder __TEXT __text $(ORDERFILE)
else
	LDAP_SECTORDER_FLAGS=
endif

apple_port:
	cp ${OBJROOT}/servers/slapd/slapd ${SYMROOT}/slapd
	for client in `ls ${OBJROOT}/clients/tools/`; do \
		cp ${OBJROOT}/clients/tools/$$client ${SYMROOT}/; \
	done
	mkdir -p $(DSTROOT)/usr/local/OpenSourceLicenses/
	cp $(SRCROOT)/OpenLDAP/LICENSE $(DSTROOT)/usr/local/OpenSourceLicenses/OpenLDAP.txt
	mkdir -p $(DSTROOT)/usr/local/OpenSourceVersions/
	cp $(SRCROOT)/AppleExtras/Resources/OpenLDAP.plist $(DSTROOT)/usr/local/OpenSourceVersions/
	mkdir -p $(DSTROOT)/System/Library/LaunchDaemons
	cp $(SRCROOT)/AppleExtras/Resources/org.openldap.slapd.plist $(DSTROOT)/System/Library/LaunchDaemons
	rm -rf ${DSTROOT}/usr/include # remove includes from server target
	rm -rf ${DSTROOT}/usr/local/include # remove local includes from server target
	rm -f ${DSTROOT}/private/etc/openldap/slapd.ldif* # remove unused files from server target

apple_framework:
	rm -rf ${OBJROOT}/libraries/liblber/*test.o
	rm -rf ${OBJROOT}/libraries/libldap/*test.o
	rm -rf ${OBJROOT}/libraries/libldap_r/*test.o
	xcrun cc ${RC_CFLAGS} -install_name /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP -compatibility_version 1.0.0 -current_version 2.4.0 \
		-o ${SYMROOT}/LDAP ${LDAP_SECTORDER_FLAGS} ${OBJROOT}/libraries/liblber/*.o ${OBJROOT}/libraries/libldap_r/*.o \
		-lsasl2 -lcrypto -lssl -lresolv ${Extra_LD_Libraries} "-Wl,-exported_symbols_list" \
		${SRCROOT}/AppleExtras/ldap.exp -twolevel_namespace -dead_strip "-Wl,-single_module" -dynamiclib
	mkdir -p ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Headers
	mkdir -p ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/PrivateHeaders
	mkdir -p ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Resources
	ln -s A  ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/Current
	ln -s Versions/Current/Headers ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Headers
	ln -s Versions/Current/PrivateHeaders ${DSTROOT}/System/Library/Frameworks/LDAP.framework/PrivateHeaders
	ln -s Versions/Current/LDAP ${DSTROOT}/System/Library/Frameworks/LDAP.framework/LDAP
	ln -s Versions/Current/Resources ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Resources
	strip -x ${SYMROOT}/LDAP -o ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/LDAP
	cp ${DSTROOT}/usr/include/ldap.h ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Headers
	cp ${DSTROOT}/usr/include/lber.h ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Headers
	cp ${DSTROOT}/usr/local/include/ldap_private.h ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/PrivateHeaders

	cp ${SRCROOT}/AppleExtras/Resources/Info.plist ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Resources
	cp ${SRCROOT}/AppleExtras/Resources/version.plist ${DSTROOT}/System/Library/Frameworks/LDAP.framework/Versions/A/Resources
	rm -rf ${DSTROOT}/usr/lib
	mkdir -p ${DSTROOT}/usr/lib
	ln -s /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP ${DSTROOT}/usr/lib/liblber.dylib
	ln -s /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP ${DSTROOT}/usr/lib/libldap.dylib
	ln -s /System/Library/Frameworks/LDAP.framework/Versions/A/LDAP ${DSTROOT}/usr/lib/libldap_r.dylib
	rm -rf ${DSTROOT}/usr/share # remove man pages from framework target
	rm -rf ${DSTROOT}/private # remove default ldap.conf from framework target
	rm -rf ${DSTROOT}/usr/bin # remove utilities from framework target
