Project = resolv
ProductType = dylib
ifeq ($(RC_ProjectName),$(RC_ProjectName:%_Sim=%))
Install_Dir = /usr/lib
else
Install_Dir = $(SDKROOT)/usr/lib
endif

HFILES = dns.h dns_private.h dns_util.h dst.h dst_internal.h\
         nameser.h res_debug.h res_private.h res_update.h resolv.h

CFILES = base64.c dns.c dns_async.c dns_util.c dst_api.c\
         dst_hmac_link.c dst_support.c ns_date.c ns_name.c ns_netint.c\
         ns_parse.c ns_print.c ns_samedomain.c ns_sign.c ns_ttl.c\
         ns_verify.c res_comp.c res_data.c res_debug.c\
         res_findzonecut.c res_init.c res_mkquery.c res_mkupdate.c\
         res_query.c res_send.c res_sendsigned.c res_update.c

# NOTE dns_plugin.c is not included in CFILES since it isn't part of the dylib

ifeq ($(RC_ProjectName),$(RC_ProjectName:%_Sim=%))
MANPAGES = resolver.3 resolver.5
endif

ifneq ($(RC_ProjectName),$(RC_ProjectName:%_Sim=%))
Install_Headers_Directory = $(SDKROOT)/usr/include
Install_Private_Headers_Directory = $(SDKROOT)/usr/local/include
endif

Install_Headers = dns.h dns_util.h nameser.h resolv.h
Install_Private_Headers = dns_private.h

Library_Version = 9

Extra_CC_Flags = -Wall -Werror -fno-common -I.

ifeq ($(RC_TARGET_CONFIG),)
        export PRODUCT := $(shell xcodebuild -sdk "$(SDKROOT)" -version PlatformPath | head -1 | sed 's,^.*/\([^/]*\)\.platform$$,\1,')
        ifeq ($(PRODUCT),)
                export PRODUCT := MacOSX
        endif
else
        export PRODUCT := $(RC_TARGET_CONFIG)
endif

ifeq "$(PRODUCT)" "iPhone"
Extra_CC_Flags += -DUSE_DNS_PSELECT
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

ifeq ($(RC_ProjectName),$(RC_ProjectName:%_Sim=%))
	INSTALL_PREFIX = 
else
	INSTALL_PREFIX = $(SDKROOT)
endif

_installhdrs:: _symlink_hdrs

_symlink_hdrs:
	$(INSTALL_DIRECTORY) $(DSTROOT)$(Install_Headers_Directory)/arpa
	$(LN) -sf ../nameser.h $(DSTROOT)$(Install_Headers_Directory)/arpa

ifeq ($(RC_ProjectName),$(RC_ProjectName:%_Sim=%))
after_install: install_man
else
after_install:
endif
	$(INSTALL_DIRECTORY) $(DSTROOT)$(Install_Headers_Directory)/arpa
	$(LN) -sf ../nameser.h $(DSTROOT)$(Install_Headers_Directory)/arpa

ifeq ($(RC_ProjectName),$(RC_ProjectName:%_Sim=%))
install_man:
	@for FILE in \
		dn_comp.3 dn_expand.3 dn_skipname.3 \
		ns_get16.3 ns_get32.3 ns_put16.3 ns_put32.3 \
		res_init.3 res_mkquery.3 res_query.3 res_search.3 res_send.3 ; do \
		$(INSTALL_FILE) resolver_so.3 $(DSTROOT)/usr/share/man/man3/$${FILE} ; \
	done
endif
