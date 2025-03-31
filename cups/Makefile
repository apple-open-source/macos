##
# Makefile for cups
##

# Project info
Project		= cups
UserType	= Administrator
ToolType	= Services

GnuNoChown      = YES
GnuAfterInstall	= install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Specify the configure flags to use...
ifeq ($(RC_MACOS),YES)
CUPS_Components=all
GSSAPI_Options=--enable-gssapi
CUPS_StaticLibrary=NO
CUPS_HasPublicHeaders=YES
CUPS_ModuleMapUnifdefOptions=-DBUILD_FOR_MACOS
else
CUPS_Components=libcupslite
GSSAPI_Options=--disable-gssapi
CUPS_StaticLibrary=YES
CUPS_HasPublicHeaders=NO
CUPS_ModuleMapUnifdefOptions=-UBUILD_FOR_MACOS
endif

Configure_Flags = `$(SRCROOT)/gettargetflags.sh host` \
		  --with-cups-build="cups-513.7" \
		  --with-adminkey="system.print.admin" \
		  --with-operkey="system.print.operator" \
		  --with-pam-module=opendirectory \
		  --with-privateinclude=/usr/local/include \
		  --with-components=$(CUPS_Components) \
		  $(GSSAPI_Options) \
		  $(Extra_Configure_Flags)

ifeq ($(CUPS_StaticLibrary),YES)
Configure_Flags += --enable-static --disable-shared --prefix=/usr/local/
endif

# Add "--enable-debug-guards" during OS development, remove for production...
#		  --enable-debug-guards \

# CUPS is able to build 1/2/3/4-way fat on its own, so don't override the
# compiler flags in make, just in configure...
Environment	=	CC=`/usr/bin/xcrun --find clang` \
			CXX=`/usr/bin/xcrun --find clang++` \
			CFLAGS="-g `$(SRCROOT)/gettargetflags.sh cflags`" \
			CPPFLAGS="`$(SRCROOT)/gettargetflags.sh cppflags`" \
			CXXFLAGS="`$(SRCROOT)/gettargetflags.sh cxxflags`" \
			LDFLAGS="`$(SRCROOT)/gettargetflags.sh ldflags`"

ifeq ($(RC_MACOS),YES)
Environment += CODE_SIGN="/usr/bin/true"
endif

ifneq ($(CUPS_StaticLibrary),YES)
Environment += DSOFLAGS="`$(SRCROOT)/gettargetflags.sh dsoflags` -dynamiclib -single_module -lc"
endif

# The default target installs programs and data files...
Install_Target	= install-data install-exec
Install_Flags	= -j`sysctl -n hw.activecpu`

# The alternate target installs libraries and header files...
#Install_Target	= install-headers install-libs
#Install_Flags =

# Shadow the source tree and force a re-configure as needed
lazy_install_source::	$(BuildDirectory)/Makedefs
	$(_v) if [ -L "$(BuildDirectory)/Makefile" ]; then \
		$(RM) "$(BuildDirectory)/Makefile"; \
		$(CP) "$(Sources)/Makefile" "$(BuildDirectory)/Makefile"; \
	fi

# Re-configure when the configure script and config.h, cups-config, or Makedefs
# templates change.
$(BuildDirectory)/Makedefs:	$(Sources)/configure $(Sources)/Makedefs.in \
				$(Sources)/config.h.in $(Sources)/cups-config.in \
				$(SRCROOT)/Makefile
	$(_v) $(RM) "$(BuildDirectory)/Makefile"
	$(_v) $(MAKE) shadow_source
	$(_v) $(RM) $(ConfigStamp)

# Install fast, without copying sources...
install-fast: $(Sources)/Makedefs
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(Sources) $(Environment) $(Install_Flags) install

install-clean: $(Sources)/Makedefs
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(Sources) $(Environment) $(Install_Flags) distclean
	$(_v) cd $(Sources) && $(Environment) LD_TRACE_FILE=/dev/null $(Configure) $(Configure_Flags)
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(Sources) $(Environment) $(Install_Flags) install

$(Sources)/Makedefs:	$(Sources)/configure $(Sources)/Makedefs.in \
			$(Sources)/config.h.in $(Sources)/cups-config.in
	@echo "Configuring $(Project) with \"$(Configure_Flags)\"..."
	$(_v) cd $(Sources) && $(Environment) LD_TRACE_FILE=/dev/null \
		$(Configure) $(Configure_Flags)

# Install everything.
install-all: configure
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) install


# Install the libraries and headers.
install-libs: configure
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) install-headers install-libs
ifeq ($(RC_MACOS),YES)
	$(_v) /usr/bin/codesign --force --sign - --entitlements $(SRCROOT)/usb-darwin.entitlements $(DSTROOT)/usr/libexec/cups/backend/usb
endif
	$(_v) umask $(Install_Mask) ; $(MAKE) $(Environment) $(Install_Flags) install-module-maps


# The plist keeps track of the open source version we ship...
OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(Project).txt


# InstallAPI stuff...
# Doesn't apply when building libcups for ios
ifneq ($(CUPS_StaticLibrary),YES)
SUPPORTS_TEXT_BASED_API ?= YES

$(info SUPPORTS_TEXT_BASED_API=$(SUPPORTS_TEXT_BASED_API))

ifeq ($(SUPPORTS_TEXT_BASED_API),YES)
install-libs: installapi-verify
endif

TAPI_INSTALL_PREFIX	=	$(DSTROOT)
TAPI_LIBRARY_PATH	=	$(TAPI_INSTALL_PREFIX)/usr/lib
TAPI_COMMON_OPTS	=	`$(SRCROOT)/gettargetflags.sh tapi` \
				-dynamiclib \
				-install_name /usr/lib/libcups.2.dylib \
				-current_version 2.14.0 \
				-compatibility_version 2.0.0 \
				-o $(OBJROOT)/cups/libcups.2.tbd

TAPI_VERIFY_OPTS	=	$(TAPI_COMMON_OPTS) \
				--verify-mode=Pedantic \
				--verify-against=$(TAPI_LIBRARY_PATH)/libcups.2.dylib
endif

installapi: configure
	@echo
	@echo ++++++++++++++++++++++
	@echo + Running InstallAPI +
	@echo ++++++++++++++++++++++
	@echo
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) libs install-headers
	$(_v) umask $(Install_Mask) ; $(MAKE) $(Environment) $(Install_Flags) install-module-maps
ifneq ($(CUPS_StaticLibrary),YES)
	xcrun --sdk $(SDKROOT) tapi installapi $(TAPI_COMMON_OPTS) $(TAPI_INSTALL_PREFIX)
	$(_v) umask $(Install_Mask) ; $(MAKE) $(Environment) $(Install_Flags) install-tbds
endif

install-tbds:
	if test -f $(OBJROOT)/cups/libcupsimage.2.dylib; then \
		xcrun --sdk $(SDKROOT) tapi stubify --set-installapi-flag $(OBJROOT)/cups/libcupsimage.2.dylib; \
	fi
	$(INSTALL) -d -m 0755 $(TAPI_LIBRARY_PATH)
	$(INSTALL) -c -m 0755 $(OBJROOT)/cups/libcups.2.tbd $(TAPI_LIBRARY_PATH)
	ln -s libcups.2.tbd $(TAPI_LIBRARY_PATH)/libcups.tbd
	if test -f $(OBJROOT)/cups/libcupsimage.2.tbd; then \
		$(INSTALL) -c -m 0755 $(OBJROOT)/cups/libcupsimage.2.tbd $(TAPI_LIBRARY_PATH); \
		ln -s libcupsimage.2.tbd $(TAPI_LIBRARY_PATH)/libcupsimage.tbd; \
	fi

installapi-verify: configure
	@echo
	@echo "+++++++++++++++++++++++++++++++"
	@echo "+ Running InstallAPI (verify) +"
	@echo "+++++++++++++++++++++++++++++++"
	@echo
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) install-libs install-headers
	$(_v) umask $(Install_Mask) ; $(MAKE) $(Environment) $(Install_Flags) install-module-maps
	xcrun --sdk $(SDKROOT) tapi installapi $(TAPI_VERIFY_OPTS) $(TAPI_INSTALL_PREFIX)
	if test -f $(OBJROOT)/cups/libcupsimage.2.dylib; then \
		xcrun --sdk $(SDKROOT) tapi stubify --set-installapi-flag $(OBJROOT)/cups/libcupsimage.2.dylib; \
	fi
	$(_v) umask $(Install_Mask) ; $(MAKE) $(Environment) $(Install_Flags) install-tbds

install-module-maps:
	-$(UNIFDEF) $(CUPS_ModuleMapUnifdefOptions) -o $(BuildDirectory)/$(Project).modulemap $(SRCROOT)/$(Project).modulemap
	-$(UNIFDEF) $(CUPS_ModuleMapUnifdefOptions) -o $(BuildDirectory)/$(Project)_private.modulemap $(SRCROOT)/$(Project)_private.modulemap
ifeq ($(CUPS_HasPublicHeaders),YES)
	$(INSTALL_FILE) $(BuildDirectory)/$(Project).modulemap $(DSTROOT)/usr/include/$(Project).modulemap
else
	$(INSTALL_FILE) $(BuildDirectory)/$(Project).modulemap $(DSTROOT)/usr/local/include/$(Project).modulemap
endif
	$(INSTALL_FILE) $(BuildDirectory)/$(Project)_private.modulemap $(DSTROOT)/usr/local/include/$(Project)_private.modulemap
