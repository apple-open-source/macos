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
Configure_Flags = --with-cups-build="cups-462.10" \
		  --with-archflags="$(RC_CFLAGS)" \
		  --with-ldarchflags="`$(SRCROOT)/getldarchflags.sh $(RC_CFLAGS)` $(PPC_FLAGS)" \
		  --with-adminkey="system.print.admin" \
		  --with-operkey="system.print.operator" \
		  --with-pam-module=opendirectory \
		  --with-privateinclude=/usr/local/include \
		  $(Extra_Configure_Flags)

# Add "--enable-debug-guards" during OS development, remove for production...
#		  --enable-debug-guards \

# CUPS is able to build 1/2/3/4-way fat on its own, so don't override the
# compiler flags in make, just in configure...
Environment	=	CC=`$(SRCROOT)/getcompiler.sh cc` \
			CXX=`$(SRCROOT)/getcompiler.sh cxx`

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
	@echo "Configuring $(Project)..."
	$(_v) cd $(Sources) && $(Environment) LD_TRACE_FILE=/dev/null \
		$(Configure) $(Configure_Flags)

# Install everything.
install-all: configure
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) install


# Install the libraries and headers.
install-libs: configure
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) install-headers install-libs


# The plist keeps track of the open source version we ship...
OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE.txt $(OSL)/$(Project).txt


# InstallAPI stuff...
SUPPORTS_TEXT_BASED_API ?= YES
$(info SUPPORTS_TEXT_BASED_API=$(SUPPORTS_TEXT_BASED_API))

ifeq ($(SUPPORTS_TEXT_BASED_API),YES)
install-libs: installapi-verify
endif

installapi: configure
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) libs
	@echo
	@echo ++++++++++++++++++++++
	@echo + Running InstallAPI +
	@echo ++++++++++++++++++++++
	@echo

	@if [ "$(SUPPORTS_TEXT_BASED_API)" != "YES" ]; then \
		echo "installapi was requested, but SUPPORTS_TEXT_BASED_API has been disabled."; \
		exit 1; \
	fi

	xcrun tapi stubify --set-installapi-flag $(OBJROOT)/cups/libcups.2.dylib
	xcrun tapi stubify --set-installapi-flag $(OBJROOT)/cgi-bin/libcupscgi.1.dylib
	xcrun tapi stubify --set-installapi-flag $(OBJROOT)/filter/libcupsimage.2.dylib
	xcrun tapi stubify --set-installapi-flag $(OBJROOT)/scheduler/libcupsmime.1.dylib
	xcrun tapi stubify --set-installapi-flag $(OBJROOT)/ppdc/libcupsppdc.1.dylib

	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/lib
	$(INSTALL) -c -m 0755 $(OBJROOT)/cups/libcups.2.tbd $(DSTROOT)/usr/lib
	ln -s libcups.2.tbd $(DSTROOT)/usr/lib/libcups.tbd
	$(INSTALL) -c -m 0755 $(OBJROOT)/cgi-bin/libcupscgi.1.tbd $(DSTROOT)/usr/lib
	ln -s libcupscgi.1.tbd $(DSTROOT)/usr/lib/libcupscgi.tbd
	$(INSTALL) -c -m 0755 $(OBJROOT)/filter/libcupsimage.2.tbd $(DSTROOT)/usr/lib
	ln -s libcupsimage.2.tbd $(DSTROOT)/usr/lib/libcupsimage.tbd
	$(INSTALL) -c -m 0755 $(OBJROOT)/scheduler/libcupsmime.1.tbd $(DSTROOT)/usr/lib
	ln -s libcupsmime.1.tbd $(DSTROOT)/usr/lib/libcupsmime.tbd
	$(INSTALL) -c -m 0755 $(OBJROOT)/ppdc/libcupsppdc.1.tbd $(DSTROOT)/usr/lib
	ln -s libcupsppdc.1.tbd $(DSTROOT)/usr/lib/libcupsppdc.tbd

installapi-verify: installapi
