Project = resolv
ProductType = dylib
Install_Dir = /usr/lib

HFILES = dns.h dns_private.h dns_util.h dst.h dst_internal.h\
         nameser.h res_debug.h res_private.h res_update.h resolv.h

CFILES = base64.c dns.c dns_async.c dns_util.c dst_api.c\
         dst_hmac_link.c dst_support.c ns_date.c ns_name.c ns_netint.c\
         ns_parse.c ns_print.c ns_samedomain.c ns_sign.c ns_ttl.c\
         ns_verify.c res_comp.c res_data.c res_debug.c\
         res_findzonecut.c res_init.c res_mkquery.c res_mkupdate.c\
         res_query.c res_send.c res_sendsigned.c res_update.c

# NOTE dns_plugin.c is not included in CFILES since it isn't part of the dylib

MANPAGES = resolver.3 resolver.5

Install_Headers = dns.h dns_util.h nameser.h resolv.h
Install_Private_Headers = dns_private.h

Library_Version = 9

Extra_CC_Flags = -Wall -Werror -fno-common -I.

PRODUCT = $(shell tconf --product)
ifeq "$(PRODUCT)" "iPhone"
Extra_CC_Flags += -DUSE_DNS_PSELECT
endif

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

build:: dns.so

PLUGIN_LD_Flags = -L$(SYMROOT) -lresolv.9
PLUGIN_CC_Flags = -bundle

PLUGIN_DEST = $(DSTROOT)/$(DESTDIR)usr/lib/info

dns.so: dns_plugin.c
	cc -c $(CFLAGS) dns_plugin.c
	cc $(PLUGIN_CC_Flags) $(LDFLAGS) $(PLUGIN_LD_Flags) -o $(SYMROOT)/dns.so dns_plugin.o
	dsymutil --out=$(SYMROOT)/dns.so.dSYM $(SYMROOT)/dns.so || exit 0
	$(INSTALL_DIRECTORY) $(PLUGIN_DEST)
	$(INSTALL_LIBRARY) $(SYMROOT)/dns.so $(PLUGIN_DEST)
	strip -S $(PLUGIN_DEST)/dns.so

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/include/arpa
	$(LN) -sf ../nameser.h $(DSTROOT)/usr/include/arpa
	@for FILE in \
		dn_comp.3 dn_expand.3 dn_skipname.3 \
		ns_get16.3 ns_get32.3 ns_put16.3 ns_put32.3 \
		res_init.3 res_mkquery.3 res_query.3 res_search.3 res_send.3 ; do \
		$(INSTALL_FILE) resolver_so.3 $(DSTROOT)/usr/share/man/man3/$${FILE} ; \
	done
