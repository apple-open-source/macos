Project		= bind9
UserType	= Developer
ToolType	= Commands
Extra_Configure_Flags = --sysconfdir="/private/etc" --localstatedir="/private/var" --enable-atomic="no"
Extra_Environment = sysconfdir="/private/etc"                               \
                    includedir="/usr/local/include"			\
                    libdir="/usr/local/"				\
                    localstatedir="/private/var"
Extra_Install_Flags = sysconfdir="/private/etc"                             \
                      includedir="$(DSTROOT)/usr/local/include"		\
                      libdir="$(DSTROOT)/usr/local/lib"                 \
                      localstatedir="/private/var"
GnuAfterInstall	= install-strip install-extra install-sandbox-profile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target	= install

install-strip:
	strip "$(DSTROOT)/usr/sbin/rndc-confgen"
	strip "$(DSTROOT)/usr/sbin/rndc"
	strip "$(DSTROOT)/usr/sbin/named-checkzone"
	strip "$(DSTROOT)/usr/sbin/named-checkconf"
	strip "$(DSTROOT)/usr/sbin/named"
	strip "$(DSTROOT)/usr/sbin/lwresd"
	strip "$(DSTROOT)/usr/sbin/dnssec-signzone"
	strip "$(DSTROOT)/usr/sbin/dnssec-signkey"
	strip "$(DSTROOT)/usr/sbin/dnssec-makekeyset"
	strip "$(DSTROOT)/usr/sbin/dnssec-keygen"
	strip "$(DSTROOT)/usr/sbin/dnssec-dsfromkey"
	strip "$(DSTROOT)/usr/sbin/dnssec-keyfromlabel"
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
install-sandbox-profile:
	mkdir -p $(DSTROOT)/usr/share/sandbox
	install -c -m 644 named.sb $(DSTROOT)/usr/share/sandbox
