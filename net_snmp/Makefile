##
# Makefile for net-snmp
##
# Project info
Project               = net-snmp
UserType              = Administration
ToolType              = Commands
GnuAfterInstall       = do-fixups install-startup install-plist
Extra_Configure_Flags = --with-libwrap --with-defaults --prefix=/usr --with-persistent-directory=/var/db/net-snmp --with-mib-modules=host CPPFLAGS=-I/System/Library/Frameworks/System.framework/PrivateHeaders 
Extra_Environment     = AR="$(SRCROOT)/ar.sh"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make


# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 5.2
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = NLS_TigerBuild.patch BO_darwin_snmp.patch NLS_PR-3962010.patch NLS_PR-4059242.patch

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

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


Install_Flags         = prefix=$(DSTROOT)/usr \
                        exec_prefix=$(DSTROOT)/usr \
                        bindir=$(DSTROOT)/usr/bin \
                        sbindir=$(DSTROOT)/usr/sbin \
			sysconfdir=$(DSTROOT)/etc \
			datadir=$(DSTROOT)/usr/share \
			includedir=$(DSTROOT)/usr/include/net-snmp \
			libdir=$(DSTROOT)/usr/lib \
			libexecdir=$(DSTROOT)/usr/libexec \
			localstatedir=$(DSTROOT)/usr/share \
        		mandir=$(DSTROOT)/usr/share/man \
		        infodir=$(DSTROOT)/usr/share/info

Install_Target = install 

do-fixups:
	for foo in encode_keychange snmpbulkget snmpbulkwalk snmpdelta snmpdf snmpget snmpgetnext snmpinform snmpnetstat snmpset snmpstatus snmptable snmptest snmptranslate snmptrap snmpusm snmpvacm snmpwalk; \
	do \
		strip $(DSTROOT)/usr/bin/$${foo}; \
	done
	for foo in snmpd snmptrapd; \
	do \
		strip $(DSTROOT)/usr/sbin/$${foo}; \
	done
	for foo in libnetsnmp libnetsnmpagent libnetsnmphelpers \
libnetsnmpmibs; \
	do \
		strip -x $(DSTROOT)/usr/lib/$${foo}.5.2.0.dylib; \
		rm -f $(DSTROOT)/usr/lib/$${foo}.5.dylib; \
		mv $(DSTROOT)/usr/lib/$${foo}.5.2.0.dylib $(DSTROOT)/usr/lib/$${foo}.5.dylib; \
		ln -s $${foo}.5.dylib $(DSTROOT)/usr/lib/$${foo}.5.2.0.dylib; \
	done
	find  $(DSTROOT)/usr/include/net-snmp -type f | xargs chmod 644
	find  $(DSTROOT)/usr/share/snmp -type f| xargs chmod 644
	rm -f $(DSTROOT)/usr/lib/*.a $(DSTROOT)/usr/lib/*.la
	ln -s net-snmp $(DSTROOT)/usr/include/ucd-snmp 
	find $(DSTROOT)/usr/share/man/ -type f | xargs chmod 644
	mkdir -p $(DSTROOT)/private/etc/
	cp snmpd.conf $(DSTROOT)/private/etc/snmpd.conf
	mv $(DSTROOT)/usr/bin/net-snmp-config $(DSTROOT)/usr/bin/net-snmp-config.old
	cat $(DSTROOT)/usr/bin/net-snmp-config.old | sed "s/-arch ppc//g" | sed "s/-arch i386//g" > $(DSTROOT)/usr/bin/net-snmp-config
	chmod 755 $(DSTROOT)/usr/bin/net-snmp-config
	rm -f $(DSTROOT)/usr/bin/net-snmp-config.old

install-startup:
	@mkdir -p $(DSTROOT)/System/Library/StartupItems/SNMP/Resources/English.lproj
	$(INSTALL) -c -m 555 $(SRCROOT)/SNMP $(DSTROOT)/System/Library/StartupItems/SNMP
	$(INSTALL) -c -m 444 $(SRCROOT)/StartupParameters.plist $(DSTROOT)/System/Library/StartupItems/SNMP
	$(INSTALL) -c -m 444 $(SRCROOT)/Localizable.strings $(DSTROOT)/System/Library/StartupItems/SNMP/Resources/English.lproj

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/net_snmp.plist $(OSV)/net_snmp.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/net_snmp.txt $(OSL)/net_snmp.txt

