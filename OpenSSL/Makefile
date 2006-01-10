##
# Makefile for OpenSSL
##

# Project info
Project         = openssl
ProjectName     = OpenSSL
UserType        = Developer
ToolType        = Libraries
Configure       = $(Sources)/config
GnuAfterInstall = shlibs strip install-man-pages

# config is kinda like configure
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# config is not really like configure
Configure_Flags = --prefix="$(Install_Prefix)"								\
		  --openssldir="$(NSLIBRARYDIR)/$(ProjectName)"						\
		  --install_prefix="$(DSTROOT)" no-idea no-asm

Environment     = CFLAG="$(CFLAGS) -DOPENSSL_NO_IDEA -DFAR="						\
		  AR="$(SRCROOT)/ar.sh r"								\
		  PERL='/usr/bin/perl'									\
		  INCLUDEDIR="$(USRDIR)/include/openssl"						\
		  MANDIR="/usr/share/man"

Install_Target  = install

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 0.9.7i
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = NLS_openssl_097b_to_097g.patch NLS_buildfailure.patch

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
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
		done
endif
	$(RMDIR) $(SRCROOT)/$(AEP_Project)/include/openssl/dh.h



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

#Version      := $(shell $(GREP) 'VERSION=' $(Sources)/Makefile.ssl | $(SED) 's/VERSION=//')
#Version      := $(shell $(GREP) "SHLIB_VERSION_NUMBER" openssl/crypto/opensslv.h | $(GREP) define | $(SED) s/\#define\ SHLIB_VERSION_NUMBER\ // | $(SED) s/\"//g)
Version	     := $(AEP_Version)
FileVersion  := $(shell echo $(Version) | sed 's/[a-z]//g')
VersionFlags := -compatibility_version $(FileVersion) -current_version $(FileVersion)
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
