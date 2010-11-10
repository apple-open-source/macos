Project               = neon
ProjectVersion        = 0.28.6
Patches               = configure.diff neon-config.in.diff

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# 7337914
unexport KRB5_CONFIG

CONFIGURE_ENV  = CFLAGS="$(RC_CFLAGS) $(CC_Debug) $(CC_Optimize)"
CONFIGURE_ARGS = --prefix=/usr \
                 --enable-shared --disable-static \
                 --with-expat --with-ssl \
                 --bindir=/usr/local/bin \
                 --mandir=/usr/local/share/man \
                 --datadir=/usr/local/share \
                 --includedir=/usr/local/include

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install::
	cd $(OBJROOT) && $(CONFIGURE_ENV) $(SRCROOT)/$(Project)/configure $(CONFIGURE_ARGS)
	ed - $(OBJROOT)/config.h < $(SRCROOT)/files/fix_config.h.ed

	$(MAKE) -C $(OBJROOT)

	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT) pkgconfigdir=/usr/local/lib/pkgconfig
	ed - $(DSTROOT)/usr/local/bin/neon-config < $(SRCROOT)/files/remove_arch_flags.ed
	ed - $(DSTROOT)/usr/local/lib/pkgconfig/neon.pc < $(SRCROOT)/files/remove_arch_flags.ed

	$(MKDIR) $(SYMROOT)/usr/lib
	$(CP) $(DSTROOT)/usr/lib/libneon.27.dylib $(SYMROOT)/usr/lib
	$(STRIP) -S $(DSTROOT)/usr/lib/libneon.27.dylib

	$(MV) $(DSTROOT)/usr/lib/libneon.la $(DSTROOT)/usr/local/lib/libneon.la

	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/src/COPYING.LIB $(OSL)/$(Project).txt

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.gz
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(Patches); do \
		patch -p0 -F0 -i $(SRCROOT)/files/$$patchfile || exit 1; \
	done
