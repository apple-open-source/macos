##
# Makefile for ntp
##

# Project info
Project           = ntp
UserType          = Administration
ToolType          = Services
Extra_Configure_Flags = --disable-nls --disable-dependency-tracking --with-crypto=openssl
Extra_Environment = AUTOCONF="$(Sources)/missing autoconf"	\
                    AUTOHEADER="$(Sources)/missing autoheader"	\
                    ac_cv_decl_syscall=no			\
                    ac_cv_header_netinfo_ni_h=no		\
                    ac_cv_func_ctty_for_f_setown=yes            \
                    ac_cv_func_mlockall=no			\
		    ac_cv_func_kvm_open=no			\
                    LIBMATH=""
Extra_CC_Flags    = -mdynamic-no-pic
Extra_LD_Flags    = -framework IOKit
GnuAfterInstall   = install-man-pages rm-tickadj classic-install-path install-sym rm-ntp-wait install-plist install-doc install-sandbox-profile install-launchd-items

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

MAN5PAGES = man/ntp.conf.5
MAN8PAGES = man/ntp-keygen.8 \
	man/ntpd.8 \
	man/ntpdate.8 \
	man/ntpdc.8 \
	man/ntpq.8 \
	man/ntptrace.8

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 4.2.2
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
#----------------------------------------------------#
#>>> Update ntp.plist when you change AEP_Patches <<<#
#----------------------------------------------------#
AEP_Patches    = NLS_EAP.patch NLS_PR-4237140.patch libmd.patch iokit.patch PR-4108417.diff

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
	    echo Applying $$patchfile; \
	    cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
	mv $(SRCROOT)/$(AEP_Project)/ntpd/ntp_intres.c $(SRCROOT)/$(AEP_Project)/ntpd/ntp_intres.c.orig
	cat $(SRCROOT)/getaddrinfo.c $(SRCROOT)/$(AEP_Project)/ntpd/ntp_intres.c.orig > $(SRCROOT)/$(AEP_Project)/ntpd/ntp_intres.c
endif

install-man-pages:
	$(INSTALL) -d $(DSTROOT)/usr/share/man/man5
	$(INSTALL) -d $(DSTROOT)/usr/share/man/man8
	$(INSTALL) -m 444 $(SRCROOT)/$(MAN5PAGES) $(DSTROOT)/usr/share/man/man5/
	$(INSTALL) -m 444 $(SRCROOT)/$(MAN8PAGES) $(DSTROOT)/usr/share/man/man8/

# since the configure script is totally *broken*
# w/ regards to enabling/disabling tickadj, we remove it
# after-the-fact, by hand.
rm-tickadj:
	$(RM) $(DSTROOT)/usr/bin/tickadj


classic-install-path:
	$(INSTALL) -d $(DSTROOT)/usr/sbin
	$(MV) $(DSTROOT)/usr/bin/ntpd $(DSTROOT)/usr/sbin/ntpd
	$(MV) $(DSTROOT)/usr/bin/ntpdate $(DSTROOT)/usr/sbin/ntpdate
	$(MV) $(DSTROOT)/usr/bin/ntpdc $(DSTROOT)/usr/sbin/ntpdc
	$(MV) $(DSTROOT)/usr/bin/ntptrace $(DSTROOT)/usr/sbin/ntptrace

install-sym:
	$(MV) $(OBJROOT)/ntpd/ntpd $(SYMROOT)
	$(MV) $(OBJROOT)/ntpdate/ntpdate $(SYMROOT)
	$(MV) $(OBJROOT)/ntpdc/ntpdc $(SYMROOT)
	$(MV) $(OBJROOT)/ntpq/ntpq $(SYMROOT)

rm-ntp-wait:
	$(RM) $(DSTROOT)/usr/bin/ntp-wait

install-plist:
	$(INSTALL) -d $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL) -d $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYRIGHT $(OSL)/$(Project).txt

install-doc:
	$(INSTALL) -d $(DSTROOT)/usr/share/doc/$(Project)
	cd $(SRCROOT)/$(Project)/html && pax -rw . $(DSTROOT)/usr/share/doc/$(Project)
	find $(DSTROOT)/usr/share/doc/$(Project) -type f -perm +0111 | xargs chmod a-x

install-sandbox-profile:
	$(INSTALL) -d $(DSTROOT)/usr/share/sandbox
	$(INSTALL) -m 644 ntpd.sb $(DSTROOT)/usr/share/sandbox

install-launchd-items:
	$(INSTALL) -d $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL) -m 644 org.ntp.ntpd.plist $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL) -d $(DSTROOT)/usr/libexec
	$(INSTALL) -m 755 ntpd-wrapper $(DSTROOT)/usr/libexec
	$(INSTALL) -d $(DSTROOT)/usr/share/man/man8
	$(INSTALL) -m 644 ntpd-wrapper.8 $(DSTROOT)/usr/share/man/man8
