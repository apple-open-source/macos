/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id$ */

#ifndef GSSKRB5_LOCL_H
#define GSSKRB5_LOCL_H

#include <config.h>

#include <gssapi_rewrite.h>

#include <krb5_locl.h>
#include <gkrb5_err.h>
#include <gssapi.h>
#include <gssapi_mech.h>
#include <gssapi_krb5.h>
#include <gssapi_spi.h>
#include <assert.h>

#include <heimbase.h>

#include <pku2u_asn1.h>
#include <gssapi_asn1.h>

#include <gsskrb5_crypto.h>

#include "cfx.h"

#ifdef __APPLE_PRIVATE__
#include <CommonCrypto/CommonCryptor.h>
#ifndef __APPLE_TARGET_EMBEDDED__
#include <CommonCrypto/CommonCryptorSPI.h>
#endif
#endif

typedef struct gsskrb5_ctx *gsskrb5_ctx;
typedef struct gsskrb5_cred *gsskrb5_cred;

typedef OM_uint32
(*gsskrb5_acceptor_state)(OM_uint32 *minor_status,
			  gsskrb5_ctx ctx,
			  krb5_context context,
			  const gss_cred_id_t acceptor_cred_handle,
			  const gss_buffer_t input_token_buffer,
			  const gss_channel_bindings_t input_chan_bindings,
			  gss_name_t * src_name,
			  gss_OID * mech_type,
			  gss_buffer_t output_token,
			  OM_uint32 * ret_flags,
			  OM_uint32 * time_rec,
			  gss_cred_id_t * delegated_cred_handle);

typedef OM_uint32
(*gsskrb5_initator_state)(OM_uint32 * minor_status,
			  gsskrb5_cred cred,
			  gsskrb5_ctx ctx,
			  krb5_context context,
			  gss_name_t name,
			  const gss_OID mech_type,
			  OM_uint32 req_flags,
			  OM_uint32 time_req,
			  const gss_channel_bindings_t input_chan_bindings,
			  const gss_buffer_t input_token,
			  gss_buffer_t output_token,
			  OM_uint32 * ret_flags,
			  OM_uint32 * time_rec);

/*
 *
 */

struct gss_msg_order;

struct gsskrb5_ctx {
  struct gsskrb5_crypto gk5c;
  gss_OID mech;
  struct krb5_auth_context_data *auth_context;
  struct krb5_auth_context_data *deleg_auth_context;
  krb5_principal source, target;
#define IS_DCE_STYLE(ctx) (((ctx)->flags & GSS_C_DCE_STYLE) != 0)
    OM_uint32 flags;
    enum { LOCAL			= 0x001,
	   OPEN				= 0x002,
	   COMPAT_OLD_DES3		= 0x004,
	   COMPAT_OLD_DES3_SELECTED	= 0x008,
	   CLOSE_CCACHE 		= 0x010,
	   DESTROY_CCACHE		= 0x020,
	   IS_CFX 			= 0x040,
	   PAC_VALID		        = 0x080,
	   RETRIED_SKEW			= 0x100,
	   RETRIED_NEWTICKET		= 0x200,
    } more_flags;
    gsskrb5_acceptor_state acceptor_state;
    gsskrb5_initator_state initiator_state;
    krb5_creds *kcred;
    krb5_ccache ccache;
    struct krb5_ticket *ticket;
    time_t endtime;
    HEIMDAL_MUTEX ctx_id_mutex;
    krb5_keyblock *service_keyblock;
    krb5_data fwd_data;
#ifdef PKINIT
    hx509_cert cert;
#endif
    krb5_storage *messages;

    /* IAKERB */
    krb5_get_init_creds_opt *gic_opt;
    krb5_init_creds_context asctx;
    krb5_tkt_creds_context tgsctx;
    krb5_data *cookie;
    char *password;
    krb5_realm iakerbrealm;
    krb5_data friendlyname;
    krb5_data lkdchostname;

};

struct gsskrb5_cred {
    krb5_principal principal;
    int cred_flags;
#define GSS_CF_DESTROY_CRED_ON_RELEASE	1
#define GSS_CF_NO_CI_FLAGS		2
#define GSS_CF_IAKERB_RESOLVED		4
    struct krb5_keytab_data *keytab;
    time_t endtime;
    gss_cred_usage_t usage;
    struct krb5_ccache_data *ccache;
    HEIMDAL_MUTEX cred_id_mutex;
    krb5_enctype *enctypes;
#ifdef PKINIT
    hx509_cert cert;
#endif
    char *password;
};

typedef struct Principal *gsskrb5_name;

/*
 *
 */

extern krb5_keytab _gsskrb5_keytab;
extern HEIMDAL_MUTEX gssapi_keytab_mutex;

/*
 * Prototypes
 */

#include <gsskrb5-private.h>

#define GSSAPI_KRB5_INIT(ctx) do {				\
    krb5_error_code kret_gss_init;				\
    if((kret_gss_init = _gsskrb5_init (ctx)) != 0) {		\
	*minor_status = kret_gss_init;				\
	return GSS_S_FAILURE;					\
    }								\
} while (0)

#define GSSAPI_KRB5_INIT_GOTO(ctx,_label) do {			\
    krb5_error_code kret_gss_init;				\
    if((kret_gss_init = _gsskrb5_init (ctx)) != 0)		\
	goto _label;						\
} while (0)

#define GSSAPI_KRB5_INIT_VOID(ctx) do {				\
    krb5_error_code kret_gss_init;				\
    if((kret_gss_init = _gsskrb5_init (ctx)) != 0)		\
	return;							\
} while (0)

#define GSSAPI_KRB5_INIT_STATUS(ctx, status) do {		\
    krb5_error_code kret_gss_init;				\
    if((kret_gss_init = _gsskrb5_init (ctx)) != 0)		\
	return GSS_S_FAILURE;					\
} while (0)


/* sec_context flags */

#define SC_LOCAL_ADDRESS  0x01
#define SC_REMOTE_ADDRESS 0x02
#define SC_KEYBLOCK	  0x04
#define SC_LOCAL_SUBKEY	  0x08
#define SC_REMOTE_SUBKEY  0x10

/* type to signal that that dns canon maybe should be done */
#define MAGIC_HOSTBASED_NAME_TYPE 4711

extern heim_string_t _gsskrb5_kGSSICPassword;
extern heim_string_t _gsskrb5_kGSSICKerberosCacheName;
extern heim_string_t _gsskrb5_kGSSICCertificate;
extern heim_string_t _gsskrb5_kGSSICLKDCHostname;
extern heim_string_t _gsskrb5_kGSSICAppIdentifierACL;


#endif
