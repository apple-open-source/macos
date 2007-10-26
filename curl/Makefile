##
# Makefile for curl
##

# Project info
Project           = curl
UserType          = Developer
ToolType          = Commands
GnuAfterInstall   = install-fixup install-plist compat-symlink move-static
GnuNoBuild        = YES

Extra_Configure_Flags = --with-gssapi=/usr

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Passing LDFLAGS to make causes GSSAPI to fail. Passing nothing works fine.
build::
	$(_v) $(MAKE) -C $(BuildDirectory)

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 7.16.3
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = configure.diff curl-config.in.diff \
                 docs__curl.1.diff src__Makefile.in.diff \
                 lib__md5.c.diff PR5101820.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

##---------------------------------------------------------------------
# Patch lib/config.h and src/config.h just after running configure
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	ed - $(OBJROOT)/lib/config.h < $(SRCROOT)/patches/config.h.ed
	ed - $(OBJROOT)/src/config.h < $(SRCROOT)/patches/config.h.ed
	touch $(ConfigStamp2)

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif

install-fixup:
	@for arch in ppc64 x86_64; do \
		file=$(DSTROOT)/usr/bin/curl; \
		echo lipo -remove $${arch} -output $${file} $${file}; \
		lipo -remove $${arch} -output $${file} $${file} || true; \
	done
	$(RM) $(DSTROOT)/usr/lib/libcurl.la
	$(STRIP) -S $(DSTROOT)/usr/lib/libcurl.a
	$(RM) $(DSTROOT)/usr/lib/libcurl.dylib $(DSTROOT)/usr/lib/libcurl.4.dylib
	$(MV) $(DSTROOT)/usr/lib/libcurl.4.0.0.dylib $(DSTROOT)/usr/lib/libcurl.4.dylib
	$(LN) -fs libcurl.4.dylib $(DSTROOT)/usr/lib/libcurl.dylib
	$(LN) -fs libcurl.4.dylib $(DSTROOT)/usr/lib/libcurl.4.0.0.dylib

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

move-static:
	$(MKDIR) $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libcurl.a $(DSTROOT)/usr/local/lib
