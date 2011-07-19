#!/bin/sh


source=$1
name=$(basename $1 | sed 's/[._-]/_/')
target=$2

tmp=$(mktemp /tmp/update-header-XXXXXX)
if [ "$tmp" = "" ] ; then
    echo "no tmpfile"
    exit q
fi

echo "$name $source $target"

cat > $tmp <<EOF
struct krb5_dh_moduli;
struct _krb5_krb_auth_data;
struct AlgorithmIdentifier;
struct key_data;
struct checksum_type;
struct key_type;

#define KRB5_DEPRECATED
#define GSSAPI_DEPRECATED
#define HC_DEPRECATED
#define HC_DEPRECATED_CRYPTO

#include <config.h>
#include <krb5.h>
#include <krb5_asn1.h>
#include "crypto-headers.h"
#include <gssapi_rewrite.h>
#include <gssapi.h>
#include <gssapi_krb5.h>
#include <gssapi_scram.h>
#include <gssapi_spnego.h>
#include <gssapi_ntlm.h>
#include <gssapi_netlogon.h>
#include <gssapi_spi.h>
#include <heimbase.h>
#include <heimbasepriv.h>
#include <hx509.h>
#include <krb5-private.h>
#include <roken.h>
#include <rtbl.h>
#include <parse_bytes.h>
#include <krb5_err.h>
#include <heim_err.h>
#include <krb_err.h>
#include <hdb_err.h>
#include <hx509_err.h>
#include <heim-ipc.h>
#include <wind.h>
#include <parse_units.h>
#include <parse_time.h>
#include <base64.h>
#include <hex.h>
#include <com_err.h>
#include <der.h>
#include <rfc2459_asn1.h>
#include <cms_asn1.h>
#include <spnego_asn1.h>
#include <gkrb5_err.h>

krb5_error_code _gsskrb5_init (krb5_context *);

extern int _krb5_AES_string_to_default_iterator;

struct hx509_collector;
struct hx_expr;
struct hx509_generate_private_context;
struct hx509_keyset_ops;
enum hx_expr_op;
typedef struct hx509_path hx509_path;
typedef void (*_hx509_cert_release_func)(struct hx509_cert_data *, void *);
typedef struct hx509_private_key_ops hx509_private_key_ops;

#include <hx509-private.h>

extern const void *${name}_export[];

const void *${name}_export[] = {
EOF
egrep -v '^ *#' $1 | sed -e 's/\([^ 	]*\)\([ 	]*,private\)*$/\1,/' | sed -e 's/^%\(.*\),$/#\1/' >> $tmp

cat >> $tmp <<EOF
NULL
};

EOF

if cmp -s "$tmp" "$target" ; then
    rm "$tmp"
else
    mv "$tmp" "$target"
fi

exit 0
