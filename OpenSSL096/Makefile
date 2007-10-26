##
# Makefile for OpenSSL
##

# Project info
Project         = openssl
ProjectName     = OpenSSL
UserType        = Developer
ToolType        = Libraries
Configure       = $(Sources)/config
GnuAfterInstall = shlibs strip 



# config is kinda like configure
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# config is not really like configure
Configure_Flags = --prefix="$(Install_Prefix)"								\
		  --openssldir="$(NSLIBRARYDIR)/$(ProjectName)"						\
		  --install_prefix="$(DSTROOT)" no-idea

Environment     = CFLAG="$(CFLAGS) -DNO_IDEA"									\
		  AR="$(SRCROOT)/ar.sh r"								\
		  PERL='/usr/bin/perl'									\
		  INCLUDEDIR="$(USRDIR)/include/openssl"						\
		  MANDIR="/usr/share/man"								\
		  PATH=$$PATH:/usr/X11R6/bin								\
		  LIBCRYPTO=../libcrypto.a LIBSSL=../libssl.a

Install_Target  = install

ORDERFILE_CRYPTO=/AppleInternal/OrderFiles/libcrypto.order
ifeq "$(shell test -f $(ORDERFILE_CRYPTO) && echo YES )" "YES"
       ORDERFLAGS_CRYPTO=-sectorder __TEXT __text $(ORDERFILE_CRYPTO)
endif

ORDERFILE_SSL=/AppleInternal/OrderFiles/libssl.order
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
	$(MAKE) -C "$(BuildDirectory)" test

configure::
	$(MAKE) -C "$(BuildDirectory)" PATH=$$PATH:/usr/X11R6/bin depend


#Version      := $(shell $(GREP) 'VERSION=' $(Sources)/Makefile.ssl | $(SED) 's/VERSION=//')
Version      := $(shell $(GREP) "SHLIB_VERSION_NUMBER" openssl/crypto/opensslv.h | $(GREP) define | $(SED) s/\#define\ SHLIB_VERSION_NUMBER\ // | $(SED) s/\"//g)
FileVersion  := $(shell echo $(Version) | $(SED) 's/^\([^\.]*\.[^\.]*\)\..*$$/\1/')
VersionFlags := -compatibility_version $(FileVersion) -current_version $(shell echo $(Version) | sed 's/[a-z]//g')
CC_Shlib      = $(CC) $(CC_Archs) -dynamiclib $(VersionFlags) -all_load

shlibs:
	@echo "Building shared libraries..."
	$(_v) $(CC_Shlib) "$(DSTROOT)$(USRLIBDIR)/libcrypto.a"						\
		-install_name "$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"				\
		$(ORDERFLAGS_CRYPTO)									\
		-o "$(DSTROOT)$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"
	$(_v) $(CC_Shlib) "$(DSTROOT)$(USRLIBDIR)/libssl.a"						\
		"$(DSTROOT)$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"					\
		-install_name "$(USRLIBDIR)/libssl.$(FileVersion).dylib"				\
		$(ORDERFLAGS_SSL)									\
		-o "$(DSTROOT)$(USRLIBDIR)/libssl.$(FileVersion).dylib"
	$(_v) for lib in crypto ssl; do								\
		$(LN) -fs "lib$${lib}.$(FileVersion).dylib" "$(DSTROOT)$(USRLIBDIR)/lib$${lib}.dylib";	\
		$(RM) "$(DSTROOT)$(USRLIBDIR)/lib$${lib}.a";						\
	      done

strip:
	$(_v) $(STRIP) -S $(shell $(FIND) $(DSTROOT)$(USRLIBDIR) -type f)
	$(_v) find $(DSTROOT) ! -type d -and ! -name libcrypto.0.9.dylib -and ! -name libssl.0.9.dylib -print0 | xargs -0 rm -f
	$(_v) rm -rf $(DSTROOT)/System
	$(_v) rm -rf $(DSTROOT)/usr/bin
	$(_v) rm -rf $(DSTROOT)/usr/include
	$(_v) rm -rf $(DSTROOT)/usr/share
