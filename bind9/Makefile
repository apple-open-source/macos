Project		= bind9
UserType	= Developer
ToolType	= Commands
Extra_CC_Flags  = -gdwarf-2 -D__APPLE_USE_RFC_2292
Extra_Configure_Flags = --prefix="/usr" --sysconfdir="/private/etc" --localstatedir="/private/var" --enable-atomic="no" \
                        --with-openssl=yes --with-gssapi=yes --enable-symtable=none

Extra_Environment = sysconfdir="/private/etc"                               \
                    includedir="/usr/local/include"			\
                    libdir="/usr/local/"				\
                    localstatedir="/private/var" \
                    prefix="/usr" \
                    mandir="/usr/share/man" \
                    DESTDIR="$(DSTROOT)"

Extra_Install_Flags = sysconfdir="/private/etc"                             \
                      includedir="/usr/local/include"		\
                      libdir="/usr/local/lib"                 \
                      localstatedir="/private/var" \
                      prefix="/usr" \
                      mandir="/usr/share/man" \
                      DESTDIR="$(DSTROOT)"

GnuAfterInstall	= gen-dsyms install-strip install-extra install-sandbox-profile

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target	= install

gen-dsyms:
	$(CP) "$(DSTROOT)/usr/sbin/rndc-confgen" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/rndc" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/named-checkzone" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/named-checkconf" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/named-compilezone" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/named-journalprint" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/named" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/lwresd" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/dnssec-signzone" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/dnssec-keygen" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/dnssec-dsfromkey" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/dnssec-keyfromlabel" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/dnssec-revoke" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/dnssec-settime" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/arpaname" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/ddns-confgen" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/genrandom" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/isc-hmac-fixup" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/sbin/nsec3hash" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/bin/nsupdate" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/bin/nslookup" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/bin/host" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/bin/dig" "$(SYMROOT)"

install-strip:
	strip "$(DSTROOT)/usr/sbin/rndc-confgen"
	strip "$(DSTROOT)/usr/sbin/rndc"
	strip "$(DSTROOT)/usr/sbin/named-checkzone"
	strip "$(DSTROOT)/usr/sbin/named-checkconf"
	strip "$(DSTROOT)/usr/sbin/named-compilezone"
	strip "$(DSTROOT)/usr/sbin/named-journalprint"
	strip "$(DSTROOT)/usr/sbin/named"
	strip "$(DSTROOT)/usr/sbin/lwresd"
	strip "$(DSTROOT)/usr/sbin/dnssec-signzone"
	strip "$(DSTROOT)/usr/sbin/dnssec-keygen"
	strip "$(DSTROOT)/usr/sbin/dnssec-dsfromkey"
	strip "$(DSTROOT)/usr/sbin/dnssec-keyfromlabel"
	strip "$(DSTROOT)/usr/sbin/dnssec-revoke"
	strip "$(DSTROOT)/usr/sbin/dnssec-settime"
	strip "$(DSTROOT)/usr/sbin/arpaname"
	strip "$(DSTROOT)/usr/sbin/ddns-confgen"
	strip "$(DSTROOT)/usr/sbin/genrandom"
	strip "$(DSTROOT)/usr/sbin/isc-hmac-fixup"
	strip "$(DSTROOT)/usr/sbin/nsec3hash"
	strip "$(DSTROOT)/usr/bin/nsupdate"
	strip "$(DSTROOT)/usr/bin/nslookup"
	strip "$(DSTROOT)/usr/bin/host"
	strip "$(DSTROOT)/usr/bin/dig"

install-extra:
	mkdir -p $(DSTROOT)$(NSLIBRARYDIR)/LaunchDaemons
	install -c -m 644 org.isc.named.plist $(DSTROOT)$(NSLIBRARYDIR)/LaunchDaemons
	mkdir -p $(DSTROOT)$(ETCDIR)
	install -c -m 644 named.conf $(DSTROOT)$(ETCDIR)                        
	mkdir -p $(DSTROOT)$(VARDIR)/named
	install -c -m 644 named.ca $(DSTROOT)$(VARDIR)/named
	install -c -m 644 named.local $(DSTROOT)$(VARDIR)/named
	install -c -m 644 localhost.zone $(DSTROOT)$(VARDIR)/named
	mkdir -p $(DSTROOT)/usr/local/share/man/
	mv $(DSTROOT)/usr/share/man/man3 $(DSTROOT)/usr/local/share/man/
	mkdir -p $(DSTROOT)/usr/local/OpenSourceVersions
	install -c -m 644 bind9.plist $(DSTROOT)/usr/local/OpenSourceVersions
	mkdir -p $(DSTROOT)/usr/local/OpenSourceLicenses
	install -c -m 644 bind9/COPYRIGHT $(DSTROOT)/usr/local/OpenSourceLicenses/bind9.txt
	rmdir $(DSTROOT)/private/var/run

install-sandbox-profile:
	mkdir -p $(DSTROOT)/usr/share/sandbox
	install -c -m 644 named.sb $(DSTROOT)/usr/share/sandbox
