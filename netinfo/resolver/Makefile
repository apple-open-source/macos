NAME = resolv

PROJECTVERSION = 2.8
PROJECT_TYPE = Library

HFILES = dns.h dns_private.h dns_util.h dst.h dst_internal.h\
	nameser.h res_debug.h res_private.h res_update.h resolv.h

CFILES = base64.c dns.c dns_async.c dns_util.c dst_api.c dst_hmac_link.c\
	dst_support.c ns_date.c ns_name.c ns_netint.c ns_parse.c ns_print.c\
	ns_samedomain.c ns_sign.c ns_ttl.c ns_verify.c res_comp.c res_data.c\
	res_debug.c res_findzonecut.c res_init.c res_mkquery.c res_mkupdate.c\
	res_query.c res_send.c res_sendsigned.c res_update.c

OTHERSRCS = Makefile Makefile.preamble Makefile.postamble resolver.5

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
CURRENTLY_ACTIVE_VERSION = YES
DEPLOY_WITH_VERSION_NAME = 9
CODE_GEN_STYLE = DYNAMIC
MAKEFILE = library.make
PUBLIC_HEADER_DIR=/usr/include
NEXTSTEP_INSTALLDIR = /usr/lib
LIBS = 
DEBUG_LIBS = $(LIBS)
PROF_LIBS = $(LIBS)


PUBLIC_HEADERS = dns.h dns_util.h nameser.h resolv.h

PROJECT_HEADERS = dns.h dns_private.h dns_util.h\
	dst.h dst_internal.h nameser.h\
	res_debug.h res_private.h res_update.h resolv.h


WINDOWS_PUBLIC_HEADERS_DIR = LOCAL_DEVELOPER_DIR/Headers/$(NAME)

NEXTSTEP_OBJCPLUS_COMPILER = /usr/bin/cc
WINDOWS_OBJCPLUS_COMPILER = $(DEVDIR)/gcc
PDO_UNIX_OBJCPLUS_COMPILER = $(NEXTDEV_BIN)/gcc
NEXTSTEP_JAVA_COMPILER = /usr/bin/javac
WINDOWS_JAVA_COMPILER = $(JDKBINDIR)/javac.exe
PDO_UNIX_JAVA_COMPILER = $(JDKBINDIR)/javac

include $(MAKEFILEDIR)/platform.make

-include Makefile.preamble

include $(MAKEFILEDIR)/$(MAKEFILE)

-include Makefile.postamble

-include Makefile.dependencies
