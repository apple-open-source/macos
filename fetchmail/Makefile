Project               = fetchmail
UserType              = Administration
ToolType              = Services
GnuAfterInstall       = post-install install-plist
Extra_Configure_Flags = --disable-nls --with-ssl \
                       --with-kerberos5=/usr \
                       --with-gssapi=/usr/include
Extra_CC_Flags = -mdynamic-no-pic

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

# Post-install cleanup
post-install:
	@for binary in fetchmail; do \
		$(CP) $(DSTROOT)/usr/bin/$${binary} $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)/usr/bin/$${binary}; \
	done
	$(RM) $(DSTROOT)/usr/share/man/man1/fetchmailconf.1
	$(LN) -s fetchmail.1.gz $(DSTROOT)/usr/share/man/man1/fetchmailconf.1.gz

# Automatic Extract & Patch
AEP_Project    = fetchmail
AEP_Version    = 6.3.11
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
# CVE-2007-4565.diff & CVE-2008-2711.diff are not needed, they were fixed in 6.3.9
AEP_Patches    = python_config.diff null_exec.diff bind9.diff \
                 manpage.diff krb5.diff getaddrinfo_signal_unsafe.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		patch -d $(SRCROOT)/$(Project) -F0 -p1 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
