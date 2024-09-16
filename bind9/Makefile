Project		= bind9
UserType	= Developer
ToolType	= Commands
Extra_CC_Flags  = -g -std=gnu89 -fno-typed-memory-operations-experimental -fno-typed-cxx-new-delete

Extra_Configure_Flags = --prefix="/usr" --sysconfdir="/private/etc" --localstatedir="/private/var" --enable-atomic="no" \
                        --with-openssl=no --with-gssapi=yes --enable-symtable=none --with-libxml2=no
#                        --with-openssl="$(SDKROOT)/usr/local/libressl" --with-gssapi=yes --enable-symtable=none --with-libxml2=no

Extra_LD_Flags    = -framework IOKit -framework CoreFoundation

Extra_Environment = sysconfdir="/private/etc"           \
                    includedir="/usr/local/include"			\
                    libdir="/usr/local/"				\
                    localstatedir="/private/var" \
                    prefix="/usr" \
                    mandir="/usr/share/man" \
                    DESTDIR="$(DSTROOT)"

Extra_Install_Flags = sysconfdir="/private/etc"         \
                      includedir="/usr/local/include"		\
                      libdir="/usr/local/lib"                 \
                      localstatedir="/private/var" \
                      prefix="/usr" \
                      mandir="/usr/share/man" \
                      DESTDIR="$(DSTROOT)"

GnuAfterInstall	= gen-dsyms install-strip install-extra

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target	= install

gen-dsyms:
	$(CP) "$(DSTROOT)/usr/sbin/ddns-confgen" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/bin/nsupdate" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/bin/nslookup" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/bin/host" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/bin/dig" "$(SYMROOT)"
	$(CP) "$(DSTROOT)/usr/bin/delv" "$(SYMROOT)"

install-strip:
	strip "$(DSTROOT)/usr/sbin/ddns-confgen"
	strip "$(DSTROOT)/usr/bin/nsupdate"
	strip "$(DSTROOT)/usr/bin/nslookup"
	strip "$(DSTROOT)/usr/bin/host"
	strip "$(DSTROOT)/usr/bin/dig"
	strip "$(DSTROOT)/usr/bin/delv"

install-extra:
	mkdir -p $(DSTROOT)/usr/local/OpenSourceVersions
	install -c -m 644 bind9.plist $(DSTROOT)/usr/local/OpenSourceVersions
	mkdir -p $(DSTROOT)/usr/local/OpenSourceLicenses
	install -c -m 644 bind9/COPYRIGHT $(DSTROOT)/usr/local/OpenSourceLicenses/bind9.txt
	rm -rf $(DSTROOT)/usr/local/include
	rm -rf $(DSTROOT)/usr/local/lib
	rm -rf $(DSTROOT)/usr/share/man/man3
	rmdir $(DSTROOT)/private/var/run
	rmdir $(DSTROOT)/private/var	
