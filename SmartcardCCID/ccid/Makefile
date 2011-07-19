Project               = ccid
UserType              = Administrator
ToolType              = Commands
GnuAfterInstall       = post-install
Extra_Configure_Flags = --enable-usbdropdir=/usr/libexec/SmartCardServices/drivers \
	PCSC_CFLAGS="-I$(SRCROOT)/ccid/MacOSX -DRESPONSECODE_DEFINED_IN_WINTYPES_H" \
	PCSC_LIBS="-framework PCSC" \
	LIBUSB_CFLAGS="-I$(DSTROOT)/usr/include" \
	LIBUSB_LIBS="-L$(DSTROOT)/usr/lib -lusb -framework IOKit -framework CoreFoundation"

Extra_Environment     = 

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# install-strip will install stripped versions of binaries
Install_Target = install-strip

# Strip debug symbols from any dylibs that slipped through install-strip (libccid.dylib.1.3.8)
post-install:
	find $(DSTROOT)/ -name '*.dylib*' -exec strip -S \{\} \;
	find $(DSTROOT)/ -name 'libusb*' -exec rm \{\} \;
	find $(DSTROOT)/ -name 'usb*.h' -exec rm \{\} \;

# Automatic Extract & Patch
AEP_Project    = ccid
AEP_Version    = 1.3.11
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = destDirFix.patch ForceWithoutPcsc.patch ccid-info-plist.patch osxConfigure.patch # ccid-info-plist.patch ForceWithoutPcsc.patch destDirFix.patch

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/files/$$patchfile) || exit 1; \
	done
	(cd $(SRCROOT)/$(Project) && MacOSX/configure --no-configure --disable-opensc --disable-pcsclite)

