#!/bin/sh

export COLORTERM=on
COLOUR=never

FREEBSD_SRC=${FREEBSD_SRC:-/Users/tj/code/freebsd/src}

# Apple original files
APPLE_FILES=(
    dns.c
    dns.h
    dns_async.c
    dns_private.h
    dns_util.c
    dns_util.h
    dst.h
    dst_api.c
    dst_hmac_link.c
    dst_internal.h
    dst_support.c
    installapi.h
    ns_date.c
    ns_sign.c
    ns_verify.c
    res_sendsigned.c
    tests/libresolv_test.c)

# Updated files available in FreeBSD
FREEBSD_FILES=(
    include/arpa/nameser.h
    lib/libc/resolv/res_debug.h
    include/resolv.h
    lib/libc/resolv/res_private.h
    include/res_update.h
    lib/libc/net/base64.c
    lib/libc/nameser/ns_name.c
    lib/libc/nameser/ns_netint.c
    lib/libc/nameser/ns_parse.c
    lib/libc/nameser/ns_print.c
    lib/libc/nameser/ns_samedomain.c
    lib/libc/nameser/ns_ttl.c
    lib/libc/resolv/res_update.c
    lib/libc/resolv/res_findzonecut.c
    lib/libc/resolv/res_comp.c
    lib/libc/resolv/res_debug.c
    lib/libc/resolv/res_mkquery.c
    lib/libc/resolv/res_mkupdate.c
    lib/libc/resolv/res_data.c
    lib/libc/resolv/res_init.c
    lib/libc/resolv/res_send.c
    lib/libc/resolv/res_query.c
    lib/libc/net/resolver.3
    share/man/man5/resolver.5)

if [ ! -z $SHOW_APPLE_FILES ]; then
    for file in ${APPLE_FILES[@]}; do
	diff --color=$COLOUR -u /dev/null $file
    done
fi

for file in ${FREEBSD_FILES[@]}; do
    diff --color=$COLOUR -u $FREEBSD_SRC/$file `basename $file`
done
