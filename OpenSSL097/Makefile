##
# Makefile for OpenSSL
##

# Project info
Project         = openssl
ProjectName     = OpenSSL
UserType        = Developer
ToolType        = Libraries
Configure       = $(Sources)/config

#TESTING: Enable postbuild testing, when built by B&I or buildit with "-project OpenSSL"
ifeq ($(RC_ProjectName), $(ProjectName))
GnuAfterInstall = shlibs gen_symbols compat_lib_only strip install-plist test
else
GnuAfterInstall = shlibs gen_symbols compat_lib_only strip install-plist
endif

# config is kinda like configure
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

ifdef SDKROOT
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

# config is not really like configure
Configure_Flags = --prefix="$(Install_Prefix)"								\
		  --openssldir="$(NSLIBRARYDIR)/$(ProjectName)"						\
		  --install_prefix="$(DSTROOT)" no-idea no-asm no-fips threads

Environment     = CFLAG="$(CFLAGS) -g $(Extra_CC_Flags) -DOPENSSL_NO_IDEA -DFAR="			\
		  AR="$(SRCROOT)/ar.sh r"								\
		  PERL='/usr/bin/perl'									\
		  INCLUDEDIR="$(USRDIR)/include/openssl"						\
		  MANDIR="/usr/share/man"

Install_Target  = install
OPENSSL_VERS    = 0.9.7

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = $(OPENSSL_VERS)l
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = NLS_openssl_097b_to_097g.patch
AEP_Patches   += SC-64bit.patch
AEP_Patches   += DVG-4574759_manpage_tweaks.patch
AEP_Patches   += DVG-4582901_bn_manpage_tweak.patch
AEP_Patches   += DVG-4602255_overlapping_manpage_fix.patch
AEP_Patches   += NLS_noinstfips.patch NLS_PR_4296241.patch
AEP_Patches   += DVG-3874266+4862555_data_pig_fix.patch
AEP_Patches   += DVG-5770109_CVE-2007-5135_off_by_on_in_SSL_get_shared_ciphers.patch
AEP_Patches   += PR-6427442_no_O3.patch
AEP_Patches   += PR-6505782_CVE-2008-5077.patch

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
		echo $$patchfile; \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
		done
endif
	$(RM) $(SRCROOT)/$(AEP_Project)/include/openssl/dh.h
	$(_v) /usr/bin/tclsh $(SRCROOT)/StripSource.tcl $(SRCROOT)
	$(RMDIR) $(SRCROOT)/$(AEP_Project).orig



ORDERFILE_CRYPTO=/usr/local/lib/OrderFiles/libcrypto.$(OPENSSL_VERS).order
ifeq "$(shell test -f $(ORDERFILE_CRYPTO) && echo YES )" "YES"
       ORDERFLAGS_CRYPTO=-sectorder __TEXT __text $(ORDERFILE_CRYPTO)
endif

ORDERFILE_SSL=/usr/local/lib/OrderFiles/libssl.$(OPENSSL_VERS).order
ifeq "$(shell test -f $(ORDERFILE_SSL) && echo YES )" "YES"
        ORDERFLAGS_SSL=-sectorder __TEXT __text $(ORDERFILE_SSL)
endif


# Shadow the source tree
lazy_install_source:: shadow_source
	$(_v) if [ -L $(BuildDirectory)/Makefile.ssl ]; then						\
		 $(RM) "$(BuildDirectory)/Makefile.ssl";						\
		 $(CP) "$(Sources)/Makefile.ssl" "$(BuildDirectory)/Makefile.ssl";			\
		 $(RM) "$(BuildDirectory)/crypto/opensslconf.h";					\
		 $(CP) "$(Sources)/crypto/opensslconf.h" "$(BuildDirectory)/crypto/opensslconf.h";	\
		 $(LN) -s ../../perlasm "$(BuildDirectory)/crypto/des/asm/perlasm";						\
	      fi

test:: build
	@echo "Running OpenSSL post build test suite, see Makefile to disable..."
	$(MAKE) -C "$(BuildDirectory)" test

Version	     := $(AEP_Version)
FileVersion  := $(shell echo $(Version) | sed 's/[a-z]//g')
VersionFlags := -compatibility_version $(FileVersion) -current_version $(FileVersion)
CC_Shlib      = $(CC) $(CC_Archs) -g -dynamiclib $(VersionFlags) -all_load
SHLIB_NOT_LINKABLE  := -Wl,-allowable_client,'!'

gen_symbols:
	$(_v) $(CP) $(DSTROOT)$(USRLIBDIR)/libcrypto.0.9.7.dylib $(SYMROOT)
	$(_v) $(CP) $(DSTROOT)$(USRLIBDIR)/libssl.0.9.7.dylib    $(SYMROOT)
	$(_v) /usr/bin/dsymutil $(SYMROOT)/libcrypto.0.9.7.dylib
	$(_v) /usr/bin/dsymutil $(SYMROOT)/libssl.0.9.7.dylib

shlibs:
	@echo "Building shared libraries..."
	# Keep binary compatibility of openssl, but, do not allow future linking against both
	# libcrypto.$(OPENSSL_VERS).dylib and libssl.$(OPENSSL_VERS).dylib. Linking 
	# is limited using "-allowable_client".  After linking libssl (which needs to 
	# link to libcrypto), we re-link libcrypto with -allowable_client to prevent 
	# any other programs linking to it.
	$(_v) $(CC_Shlib) "$(DSTROOT)$(USRLIBDIR)/libcrypto.a"						\
		-install_name "$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"				\
		$(ORDERFLAGS_CRYPTO)									\
		-o "$(DSTROOT)$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"
	$(_v) $(CC_Shlib) "$(DSTROOT)$(USRLIBDIR)/libssl.a"						\
		"$(DSTROOT)$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"					\
		-install_name "$(USRLIBDIR)/libssl.$(FileVersion).dylib"				\
		$(ORDERFLAGS_SSL)									\
		$(SHLIB_NOT_LINKABLE)									\
		-o "$(DSTROOT)$(USRLIBDIR)/libssl.$(FileVersion).dylib"
	$(_v) $(CC_Shlib) "$(DSTROOT)$(USRLIBDIR)/libcrypto.a"						\
		-install_name "$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"				\
		$(ORDERFLAGS_CRYPTO)									\
		$(SHLIB_NOT_LINKABLE)									\
		-o "$(DSTROOT)$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"

compat_lib_only:
	$(_v) rm -rf $(DSTROOT)/System
	$(_v) rm -rf $(DSTROOT)/usr/bin
	$(_v) rm -rf $(DSTROOT)/usr/include
	$(_v) rm -rf $(DSTROOT)/usr/lib/pkgconfig
	$(_v) rm -rf $(DSTROOT)/usr/share

strip:
	$(_v) $(FIND) $(DSTROOT) ! -type d -and ! -name libcrypto.$(OPENSSL_VERS).dylib -and ! -name libssl.$(OPENSSL_VERS).dylib -print0 | xargs -0 rm -f
	$(_v) $(STRIP) -S $$($(FIND) $(DSTROOT)$(USRLIBDIR) -type f)
	$(_v) $(FIND) $(DSTROOT)$(USRLIBDIR) -type f -exec cp "{}" $(SYMROOT) \;


configure::
	$(_v) touch $(BuildDirectory)/include/openssl/idea.h

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses
PN      = OpenSSL097

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(PN).plist $(OSV)/$(PN).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(PN).txt

