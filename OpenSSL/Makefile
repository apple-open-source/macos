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
GnuAfterInstall = shlibs strip install-man-pages remove-64-openssl test
else
GnuAfterInstall = shlibs strip install-man-pages remove-64-openssl
endif

# config is kinda like configure
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# config is not really like configure
Configure_Flags = --prefix="$(Install_Prefix)"								\
		  --openssldir="$(NSLIBRARYDIR)/$(ProjectName)"						\
		  --install_prefix="$(DSTROOT)" no-idea no-asm no-fips threads

Environment     = CFLAG="$(CFLAGS) -DOPENSSL_NO_IDEA -DFAR="						\
		  AR="$(SRCROOT)/ar.sh r"								\
		  PERL='/usr/bin/perl'									\
		  INCLUDEDIR="$(USRDIR)/include/openssl"						\
		  MANDIR="/usr/share/man"

Install_Target  = install

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 0.9.7l
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = NLS_openssl_097b_to_097g.patch SC-64bit.patch DVG-4574759_manpage_tweaks.patch        \
		 DVG-4582901_bn_manpage_tweak.patch DVG-4602255_overlapping_manpage_fix.patch 		   \
		 NLS_noinstfips.patch NLS_PR_4296241.patch DVG-3874266+4862555_data_pig_fix.patch      \
		 DVG-5770109_SSL_get_shared_ciphers.patch

MANPAGES       = man/openssl_fips_fingerprint.1

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
	$(_v) cd $(SRCROOT) && /usr/bin/tclsh $(SRCROOT)/StripSource.tcl $(Project)



ORDERFILE_CRYPTO=/usr/local/lib/OrderFiles/libcrypto.0.9.7.order
ifeq "$(shell test -f $(ORDERFILE_CRYPTO) && echo YES )" "YES"
       ORDERFLAGS_CRYPTO=-sectorder __TEXT __text $(ORDERFILE_CRYPTO)
endif

ORDERFILE_SSL=/usr/local/lib/OrderFiles/libssl.0.9.7.order
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
	$(_v) touch $(BuildDirectory)/include/openssl/idea.h
#	$(_v) ed - $(BuildDirectory)/include/openssl/opensslconf.h < $(FIX)/opensslconf.h.ed

install-man-pages:
	$(LN) -s verify.1ssl $(DSTROOT)/$(MANDIR)/man1/c_rehash.1ssl
	install -d $(DSTROOT)/usr/share/man/man3
	install -c -m 444 $(SRCROOT)/$(MANPAGES) $(DSTROOT)/usr/share/man/man1/

remove-64-openssl:		#OpenSSL 64 binary is busted
	@for arch in ${RC_ARCHS}; do \
		case $$arch in \
		ppc64|x86_64) \
			echo "Deleting $$arch executable from $(DSTROOT)/usr/bin/openssl"; \
			lipo -remove $$arch $(DSTROOT)/usr/bin/openssl -output $(DSTROOT)/usr/bin/openssl;; \
		esac; \
	done
