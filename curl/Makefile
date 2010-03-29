##
# Makefile for curl
##

# Project info
Project           = curl
ProjectVersion    = 7.19.7
UserType          = Developer
ToolType          = Commands
GnuAfterInstall   = install-fixup install-plist compat-symlink strip-binaries
GnuNoBuild        = YES

Patches = configure.diff \
          docs__curl.1.diff \
          src__Makefile.in.diff \
          lib__md5.c.diff \
          libcurl.pc.in.diff \
          LDAP-5648196.patch \
          configure-5709172.patch \
          Kerberos-4258093.patch \
          DiskImages-6103805.patch \
          curl-config.in.diff \
          tests__runtests.pl.diff

Extra_Configure_Flags = --with-gssapi --enable-hidden-symbols --disable-static

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

# Passing LDFLAGS to make causes GSSAPI to fail. Passing nothing works fine.
build::
	$(_v) $(MAKE) -C $(BuildDirectory)

##---------------------------------------------------------------------
# Patch lib/config.h and src/config.h just after running configure
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	ed - $(OBJROOT)/lib/curl_config.h < $(SRCROOT)/patches/config.h.ed
	ed - $(OBJROOT)/src/curl_config.h < $(SRCROOT)/patches/config.h.ed
	ed - $(OBJROOT)/include/curl/curlbuild.h < $(SRCROOT)/patches/curlbuild.h.ed
	touch $(ConfigStamp2)

ProjVers = $(Project)-$(ProjectVersion)

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(ProjVers)
	$(TAR) -C $(SRCROOT) -xf $(SRCROOT)/$(ProjVers).tar.bz2
	$(MV) $(SRCROOT)/$(ProjVers) $(SRCROOT)/$(Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(Patches); do \
		patch -p0 -F0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done

install-fixup:
	$(RM) $(DSTROOT)/usr/lib/libcurl.la

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/COPYING $(OSL)/$(Project).txt

compat-symlink:
	$(LN) -s libcurl.4.dylib $(DSTROOT)/usr/lib/libcurl.2.dylib
	$(LN) -s libcurl.4.dylib $(DSTROOT)/usr/lib/libcurl.3.dylib

strip-binaries:
	$(MKDIR) $(SYMROOT)/usr/bin
	$(CP) $(DSTROOT)/usr/bin/curl $(SYMROOT)/usr/bin
	$(STRIP) $(DSTROOT)/usr/bin/curl

	$(MKDIR) $(SYMROOT)/usr/lib
	$(CP) $(DSTROOT)/usr/lib/libcurl.4.dylib $(SYMROOT)/usr/lib
	$(STRIP) -S $(DSTROOT)/usr/lib/libcurl.4.dylib
