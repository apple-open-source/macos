##
# Makefile for lukemftpd (tnftpd)
##


ETCDIR		    = /private/etc

# Project Info
Project             = tnftpd
Extra_CC_Flags      = -no-cpp-precomp -Os -mdynamic-no-pic
GnuNoBuild          = YES
GnuAfterInstall     = post-install install-pam-item install-plist 
Extra_Configure_Flags     += --prefix=/usr --sysconfdir="$(ETCDIR)" --enable-ipv6 --sbindir=$(DSTROOT)/usr/libexec --mandir=$(DSTROOT)/usr/share/man


# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target      = install
Install_Flags       = DESTDIR=$(DSTROOT)

build:: configure
	$(_v) $(MAKE) -C $(BuildDirectory)

post-install:
	$(STRIP) -x $(DSTROOT)/usr/libexec/tnftpd
	$(MV) $(DSTROOT)/usr/libexec/tnftpd $(DSTROOT)/usr/libexec/ftpd
	$(LN) $(DSTROOT)/usr/share/man/man5/ftpusers.5 $(DSTROOT)/usr/share/man/man5/ftpchroot.5
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/ftpd/examples
	$(INSTALL_FILE) -c -m 0644  $(SRCROOT)/$(Project)/examples/ftpd.conf $(DSTROOT)/usr/share/ftpd/examples
	$(INSTALL_FILE) -c -m 0644  $(SRCROOT)/$(Project)/examples/ftpusers $(DSTROOT)/usr/share/ftpd/examples

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL_FILE) $(SRCROOT)/ftp.plist $(DSTROOT)/System/Library/LaunchDaemons
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/lukemftpd.plist $(OSV)/lukemftpd.plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/lukemftpd.txt

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 20061217
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = PR-2571387-pw.db.patch \
		PR-3285536.pamify.patch \
		PR-3795936.long-username.patch \
	        PR-3886477.Makefile.in.patch PR-3886477.ftpd.c.patch \
		PR-3186515.ftpd.conf.5.patch PR-3186515.ftpusers.5.patch \
		PR-4570983.ftpd.conf.5.patch \
		PR-4608716.ls.c.patch \
		PR-4581099.ftpd.c.patch \
		PR-4616924.ftpd.c.patch


install-pam-item:
	$(STRIP) -x $(DSTROOT)/usr/libexec/ftpd
	$(MKDIR) $(DSTROOT)/private/etc/pam.d/
	cp ftpd $(DSTROOT)/private/etc/pam.d/

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
	    (cd $(SRCROOT)/$(Project) && patch -lp0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif
