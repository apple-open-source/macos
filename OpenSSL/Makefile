##
# Makefile for OpenSSL
##

# Project info
Project         = openssl
ProjectName     = OpenSSL
UserType        = Developer
ToolType        = Libraries
Configure       = $(Sources)/config
Extra_CC_Flags  = -Wno-precomp
GnuAfterInstall = shlibs strip install-man-pages

# config is kinda like configure
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make


# config is not really like configure
Configure_Flags = --prefix="$(Install_Prefix)"								\
		  --openssldir="$(NSLIBRARYDIR)/$(ProjectName)"						\
		  --install_prefix="$(DSTROOT)" no-idea

Environment     = CFLAG="$(CFLAGS) -DOPENSSL_NO_IDEA -DFAR="									\
		  AR="$(SRCROOT)/ar.sh r"								\
		  PERL='/usr/bin/perl'									\
		  INCLUDEDIR="$(USRDIR)/include/openssl"						\
		  MANDIR="/usr/share/man"

Install_Target  = install


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

#Version      := $(shell $(GREP) 'VERSION=' $(Sources)/Makefile.ssl | $(SED) 's/VERSION=//')
Version      := $(shell $(GREP) "SHLIB_VERSION_NUMBER" openssl/crypto/opensslv.h | $(GREP) define | $(SED) s/\#define\ SHLIB_VERSION_NUMBER\ // | $(SED) s/\"//g)
FileVersion  := $(shell echo $(Version) | sed 's/[a-z]//g')
VersionFlags := -compatibility_version $(FileVersion) -current_version $(FileVersion)
CC_Shlib      = $(CC) $(CC_Archs) -dynamiclib $(VersionFlags) -all_load

shlibs:
	@echo "Building shared libraries..."
	$(_v) $(CC_Shlib) "$(DSTROOT)$(USRLIBDIR)/libcrypto.a"						\
		-install_name "$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"				\
		-sectorder __TEXT __text /AppleInternal/OrderFiles/libcrypto.order			\
		-o "$(DSTROOT)$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"
	$(_v) $(CC_Shlib) "$(DSTROOT)$(USRLIBDIR)/libssl.a"						\
		"$(DSTROOT)$(USRLIBDIR)/libcrypto.$(FileVersion).dylib"					\
		-install_name "$(USRLIBDIR)/libssl.$(FileVersion).dylib"				\
		-sectorder __TEXT __text /AppleInternal/OrderFiles/libssl.order				\
		-o "$(DSTROOT)$(USRLIBDIR)/libssl.$(FileVersion).dylib"
	$(_v) for lib in crypto ssl; do								\
		$(LN) -fs "lib$${lib}.$(FileVersion).dylib" "$(DSTROOT)$(USRLIBDIR)/lib$${lib}.dylib";	\
		$(RM) "$(DSTROOT)$(USRLIBDIR)/lib$${lib}.a";						\
	      done

strip:
	$(_v) $(STRIP)    $(shell $(FIND) $(DSTROOT)$(USRBINDIR) -type f)
	$(_v) $(STRIP) -S $(shell $(FIND) $(DSTROOT)$(USRLIBDIR) -type f)
	for MPAGE in $(DSTROOT)/usr/share/man/man*/*; do						\
		echo $${MPAGE};										\
		if [ ! -L $${MPAGE} ];then								\
			mv $${MPAGE} $${MPAGE}ssl;							\
			continue;									\
		fi;											\
		THELINK=`ls -l $${MPAGE} | awk '{ print $$NF }'`;					\
		ln -snf $${THELINK}ssl $${MPAGE}ssl;							\
		rm -f $${MPAGE};									\
	done


configure::
	make -C $(BuildDirectory) depend

install-man-pages:
	$(LN) -s verify.1ssl $(DSTROOT)/$(MANDIR)/man1/c_rehash.1ssl
