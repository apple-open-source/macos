##
# Makefile for net-snmp
##
# Project info
Project               = net-snmp
UserType              = Administration
ToolType              = Commands
GnuAfterInstall       = do-fixups install-startup
Extra_Configure_Flags = --with-libwrap --with-defaults --prefix=/usr --with-persistent-directory=/var/db/ucd-snmp --with-mib-modules=host
Extra_Environment     = AR="$(SRCROOT)/ar.sh"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags         = prefix=$(DSTROOT)/usr \
                        exec_prefix=$(DSTROOT)/usr \
                        bindir=$(DSTROOT)/usr/bin \
                        sbindir=$(DSTROOT)/usr/sbin \
			sysconfdir=$(DSTROOT)/etc \
			datadir=$(DSTROOT)/usr/share \
			includedir=$(DSTROOT)/usr/local/include/ucd-snmp \
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
	#for foo in libsnmp-0.4.2.3.dylib libucdagent-0.4.2.3.dylib libucdmibs-0.4.2.3.dylib; \
	#do \
	#	strip -x $(DSTROOT)/usr/lib/$${foo}; \
	#done
	find  $(DSTROOT)/usr/local/include/ucd-snmp -type f | xargs chmod 644
	find  $(DSTROOT)/usr/share/snmp -type f| xargs chmod 644
	rm -f $(DSTROOT)/usr/lib/*.a $(DSTROOT)/usr/lib/*.la

install-startup:
	@mkdir -p $(DSTROOT)/System/Library/StartupItems/SNMP/Resources/English.lproj
	$(INSTALL) -c -m 555 $(SRCROOT)/SNMP $(DSTROOT)/System/Library/StartupItems/SNMP
	$(INSTALL) -c -m 444 $(SRCROOT)/StartupParameters.plist $(DSTROOT)/System/Library/StartupItems/SNMP
	$(INSTALL) -c -m 444 $(SRCROOT)/Localizable.strings $(DSTROOT)/System/Library/StartupItems/SNMP/Resources/English.lproj
