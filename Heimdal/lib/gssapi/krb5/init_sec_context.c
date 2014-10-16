/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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

#include "gsskrb5_locl.h"

#ifdef __APPLE__

#include <notify.h>
#include <notify_keys.h>
#include "kcm.h"

/*
 * Provide a negative cache that store a couple of entries in the
 * process, this will take the edge of application that are very
 * insistant on calling gss_init_sec_context().
 *
 * The cache users noticiation from KCM to know when to clear the
 * cache.
 */

struct negative_cache {
    gss_OID mech;
    krb5_principal client;
    krb5_principal server;
    OM_uint32 major;
    OM_uint32 minor;
    const char *message;
};

static HEIMDAL_MUTEX nc_mutex = HEIMDAL_MUTEX_INITIALIZER;

static struct gnegative_cache {
    int inited;
    int token_cache;
    int token_time;
    size_t next_entry;
    struct negative_cache cache[7];
} nc;

static void
free_entry(krb5_context context, struct negative_cache *e)
{
    if (e->server)
        krb5_free_principal(context, e->server);
    if (e->client)
        krb5_free_principal(context, e->client);
    if (e->message)
	krb5_free_error_message(context, e->message);
    e->client = NULL;
    e->server = NULL;
    e->message = NULL;
}

static OM_uint32
check_neg_cache(OM_uint32 *minor_status,
		krb5_context context,
		const gss_OID mech,
                gss_cred_id_t gss_cred,
		gss_name_t target_name)
{
    krb5_principal server = (krb5_principal)target_name;
    gsskrb5_cred cred = (gsskrb5_cred)gss_cred;
    OM_uint32 major;
    int check = 0;
    size_t i;

    HEIMDAL_MUTEX_lock(&nc_mutex);

    if (!nc.inited) {

        (void)notify_register_check(KRB5_KCM_NOTIFY_CACHE_CHANGED, &nc.token_cache);
	(void)notify_register_check(kNotifyClockSet, &nc.token_time);

        nc.inited = 1;
    }

    notify_check(nc.token_cache, &check);
    if (!check)
	notify_check(nc.token_time, &check);

    /* if something changed, remove cache and return success */
    if (check) {
        for (i = 0; i < sizeof(nc.cache)/sizeof(nc.cache[0]); i++)
            free_entry(context, &nc.cache[i]);
	_gss_mg_log(1, "krb5-isc: got a notification, drop negative cache");
	HEIMDAL_MUTEX_unlock(&nc_mutex);
        return GSS_S_COMPLETE;
    }

    /* check if match */
    for (i = 0; i < sizeof(nc.cache)/sizeof(nc.cache[0]); i++) {
	if (gss_oid_equal(nc.cache[i].mech, mech) == 0)
	    continue;
        if (nc.cache[i].server == NULL)
            continue;
        if (!krb5_principal_compare(context, server, nc.cache[i].server))
            continue;
        if (cred && cred->principal) {
            if (nc.cache[i].client == NULL)
                continue;
            if (!krb5_principal_compare(context, cred->principal,
					nc.cache[i].client))
                continue;
        } else if (nc.cache[i].client)
            continue;

        *minor_status = nc.cache[i].minor;
        major = nc.cache[i].major;

	_gss_mg_log(1, "gss-isc: negative cache %d/%d - %s",
		    (int)nc.cache[i].major, 
		    (int)nc.cache[i].minor,
		    nc.cache[i].message);

	krb5_set_error_message(context, *minor_status, "%s (negative cache)",
			       nc.cache[i].message);

        HEIMDAL_MUTEX_unlock(&nc_mutex);
        return major;
    }

    HEIMDAL_MUTEX_unlock(&nc_mutex);

    _gss_mg_log(1, "gss-isc: not negative cache");

    return GSS_S_COMPLETE;
}

static void
update_neg_cache(OM_uint32 major_status, OM_uint32 minor_status,
                 krb5_context context,
		 gss_OID mech,
                 gsskrb5_cred cred,
                 krb5_principal server)
{
    HEIMDAL_MUTEX_lock(&nc_mutex);

    free_entry(context, &nc.cache[nc.next_entry]);

    nc.cache[nc.next_entry].mech = mech;
    krb5_copy_principal(context, server, &nc.cache[nc.next_entry].server);
    if (cred && cred->principal) {
        krb5_copy_principal(context, cred->principal,
                            &nc.cache[nc.next_entry].client);
    }
    nc.cache[nc.next_entry].major = major_status;
    nc.cache[nc.next_entry].minor = minor_status;
    nc.cache[nc.next_entry].message = 
	krb5_get_error_message(context, minor_status);

    nc.next_entry = (nc.next_entry + 1) %
        (sizeof(nc.cache)/sizeof(nc.cache[0]));

    HEIMDAL_MUTEX_unlock(&nc_mutex);
}

#endif /* __APPLE__ */

#define GKIS(name) \
static								   \
OM_uint32 name(OM_uint32 *, gsskrb5_cred, gsskrb5_ctx,		   \
	       krb5_context, gss_name_t, const gss_OID,		   \
	       OM_uint32, OM_uint32, const gss_channel_bindings_t, \
	       const gss_buffer_t, gss_buffer_t,                   \
	       OM_uint32 *, OM_uint32 *)

#ifdef PKINIT
GKIS(init_pku2u_auth);
GKIS(step_pku2u_auth);
#endif
GKIS(init_iakerb_auth);
GKIS(step_iakerb_auth_as);
GKIS(step_iakerb_auth_tgs);
GKIS(init_krb5_auth);
GKIS(step_setup_keys);
GKIS(init_auth_step);
GKIS(wait_repl_mutual);
GKIS(step_completed);

/*
 * copy the addresses from `input_chan_bindings' (if any) to
 * the auth context `ac'
 */

static OM_uint32
set_addresses (krb5_context context,
	       krb5_auth_context ac,
	       const gss_channel_bindings_t input_chan_bindings)
{
    /* Port numbers are expected to be in application_data.value,
     * initator's port first */

    krb5_address initiator_addr, acceptor_addr;
    krb5_error_code kret;

    if (input_chan_bindings == GSS_C_NO_CHANNEL_BINDINGS
	|| input_chan_bindings->application_data.length !=
	2 * sizeof(ac->local_port))
	return 0;

    memset(&initiator_addr, 0, sizeof(initiator_addr));
    memset(&acceptor_addr, 0, sizeof(acceptor_addr));

    ac->local_port =
	*(int16_t *) input_chan_bindings->application_data.value;

    ac->remote_port =
	*((int16_t *) input_chan_bindings->application_data.value + 1);

    kret = _gsskrb5i_address_to_krb5addr(context,
					 input_chan_bindings->acceptor_addrtype,
					 &input_chan_bindings->acceptor_address,
					 ac->remote_port,
					 &acceptor_addr);
    if (kret)
	return kret;

    kret = _gsskrb5i_address_to_krb5addr(context,
					 input_chan_bindings->initiator_addrtype,
					 &input_chan_bindings->initiator_address,
					 ac->local_port,
					 &initiator_addr);
    if (kret) {
	krb5_free_address (context, &acceptor_addr);
	return kret;
    }

    kret = krb5_auth_con_setaddrs(context,
				  ac,
				  &initiator_addr,  /* local address */
				  &acceptor_addr);  /* remote address */

    krb5_free_address (context, &initiator_addr);
    krb5_free_address (context, &acceptor_addr);

#if 0
    free(input_chan_bindings->application_data.value);
    input_chan_bindings->application_data.value = NULL;
    input_chan_bindings->application_data.length = 0;
#endif

    return kret;
}

OM_uint32
_gsskrb5_create_ctx(OM_uint32 * minor_status,
		    gss_ctx_id_t * context_handle,
		    krb5_context context,
		    const gss_channel_bindings_t input_chan_bindings,
		    gss_OID mech)
{
    krb5_error_code kret;
    gsskrb5_ctx ctx;

    *context_handle = NULL;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    ctx->mech			= mech;
    ctx->auth_context		= NULL;
    ctx->deleg_auth_context	= NULL;
    ctx->source			= NULL;
    ctx->target			= NULL;
    ctx->kcred			= NULL;
    ctx->ccache			= NULL;
    ctx->flags			= 0;
    ctx->more_flags		= 0;
    ctx->service_keyblock	= NULL;
    ctx->ticket			= NULL;
    krb5_data_zero(&ctx->fwd_data);

    HEIMDAL_MUTEX_init(&ctx->ctx_id_mutex);

    kret = krb5_auth_con_init (context, &ctx->auth_context);
    if (kret) {
	*minor_status = kret;
	HEIMDAL_MUTEX_destroy(&ctx->ctx_id_mutex);
	return GSS_S_FAILURE;
    }

    kret = krb5_auth_con_init (context, &ctx->deleg_auth_context);
    if (kret) {
	*minor_status = kret;
	krb5_auth_con_free(context, ctx->auth_context);
	HEIMDAL_MUTEX_destroy(&ctx->ctx_id_mutex);
	return GSS_S_FAILURE;
    }

    kret = set_addresses(context, ctx->auth_context, input_chan_bindings);
    if (kret) {
	*minor_status = kret;

	krb5_auth_con_free(context, ctx->auth_context);
	krb5_auth_con_free(context, ctx->deleg_auth_context);

	HEIMDAL_MUTEX_destroy(&ctx->ctx_id_mutex);

	return GSS_S_BAD_BINDINGS;
    }

    kret = set_addresses(context, ctx->deleg_auth_context, input_chan_bindings);
    if (kret) {
	*minor_status = kret;

	krb5_auth_con_free(context, ctx->auth_context);
	krb5_auth_con_free(context, ctx->deleg_auth_context);

	HEIMDAL_MUTEX_destroy(&ctx->ctx_id_mutex);

	return GSS_S_BAD_BINDINGS;
    }

    /*
     * We need a sequence number
     */

    krb5_auth_con_addflags(context, ctx->auth_context,
			   KRB5_AUTH_CONTEXT_DO_SEQUENCE, NULL);

    /*
     * We need a sequence number
     */

    krb5_auth_con_addflags(context,
			   ctx->deleg_auth_context,
			   KRB5_AUTH_CONTEXT_DO_SEQUENCE,
			   NULL);

    *context_handle = (gss_ctx_id_t)ctx;

    return GSS_S_COMPLETE;
}

/*
 * On success, set ctx->kcred to a valid ticket
 */

static OM_uint32
gsskrb5_get_creds(
        OM_uint32 * minor_status,
	krb5_context context,
	krb5_ccache ccache,
	gsskrb5_ctx ctx,
	const gss_name_t target_name,
	int use_dns,
	OM_uint32 time_req,
	OM_uint32 * time_rec)
{
    OM_uint32 ret;
    krb5_error_code kret;
    krb5_creds this_cred;
    OM_uint32 lifetime_rec;

    if (ctx->target) {
	krb5_free_principal(context, ctx->target);
	ctx->target = NULL;
    }
    if (ctx->kcred) {
	krb5_free_creds(context, ctx->kcred);
	ctx->kcred = NULL;
    }

    ret = _gsskrb5_canon_name(minor_status, context, use_dns,
			      ctx->source, target_name, &ctx->target);
    if (ret)
	return ret;

    if (_krb5_have_debug(context, 1)) {
	char *str;
	ret = krb5_unparse_name(context, ctx->target, &str);
	if (ret == 0) {
	    _gss_mg_log(1, "gss-krb5: ISC server %s %s", str, 
			use_dns ? "dns" : "referals");
	    krb5_xfree(str);
	}
    }

    memset(&this_cred, 0, sizeof(this_cred));
    this_cred.client = ctx->source;
    this_cred.server = ctx->target;

    if (time_req && time_req != GSS_C_INDEFINITE) {
	krb5_timestamp ts;

	krb5_timeofday (context, &ts);
	this_cred.times.endtime = ts + time_req;
    }

    kret = krb5_get_credentials(context,
				KRB5_TC_MATCH_REFERRAL,
				ccache,
				&this_cred,
				&ctx->kcred);
    if (kret) {
	_gss_mg_log(1, "gss-krb5: ISC get cred failed with %d %s", kret, 
		    use_dns ? "dns" : "referals");
	*minor_status = kret;
	return GSS_S_FAILURE;
    }

    if (_krb5_have_debug(context, 1)) {
	char *str;
	ret = krb5_unparse_name(context, ctx->kcred->server, &str);
	if (ret == 0) {
	    _gss_mg_log(1, "gss-krb5: ISC will use %s", str);
	    krb5_xfree(str);
	}
    }

    ctx->endtime = ctx->kcred->times.endtime;

    ret = _gsskrb5_lifetime_left(minor_status, context,
				 ctx->endtime, &lifetime_rec);
    if (ret) return ret;

    if (lifetime_rec == 0) {
	_gss_mg_log(1, "gss-krb5: credentials expired");
	*minor_status = 0;
	return GSS_S_CONTEXT_EXPIRED;
    }

    if (time_rec) *time_rec = lifetime_rec;

    return GSS_S_COMPLETE;
}

static OM_uint32
initiator_ready(OM_uint32 * minor_status,
		gsskrb5_ctx ctx,
		krb5_context context,
		OM_uint32 *ret_flags)
{
    OM_uint32 ret;
    int32_t seq_number;
    int is_cfx = 0;

    krb5_free_creds(context, ctx->kcred);
    ctx->kcred = NULL;

    if (ctx->more_flags & CLOSE_CCACHE)
	krb5_cc_close(context, ctx->ccache);
    ctx->ccache = NULL;

    krb5_auth_con_getremoteseqnumber (context, ctx->auth_context, &seq_number);

    _gsskrb5i_is_cfx(context, ctx, 0);
    is_cfx = (ctx->more_flags & IS_CFX);

    ret = _gssapi_msg_order_create(minor_status,
				   &ctx->gk5c.order,
				   _gssapi_msg_order_f(ctx->flags),
				   seq_number, 0, is_cfx);
    if (ret) return ret;

    ctx->initiator_state = step_completed;
    ctx->more_flags	|= OPEN;

    if (ret_flags)
	*ret_flags = ctx->flags;

    return GSS_S_COMPLETE;
}

/*
 * handle delegated creds in init-sec-context
 */

static void
do_delegation (krb5_context context,
	       krb5_auth_context ac,
	       krb5_ccache ccache,
	       krb5_creds *cred,
	       krb5_const_principal name,
	       krb5_data *fwd_data,
	       uint32_t flagmask,
	       uint32_t *flags)
{
    krb5_creds creds;
    KDCOptions fwd_flags;
    krb5_error_code kret;

    memset (&creds, 0, sizeof(creds));
    krb5_data_zero (fwd_data);

    kret = krb5_cc_get_principal(context, ccache, &creds.client);
    if (kret)
	goto out;

    kret = krb5_make_principal(context,
			       &creds.server,
			       creds.client->realm,
			       KRB5_TGS_NAME,
			       creds.client->realm,
			       NULL);
    if (kret)
	goto out;

    creds.times.endtime = 0;

    memset(&fwd_flags, 0, sizeof(fwd_flags));
    fwd_flags.forwarded = 1;
    fwd_flags.forwardable = 1;

    if (name->name.name_string.len < 2) {
	krb5_set_error_message(context, GSS_KRB5_S_G_BAD_USAGE,
			       "ISC: only support forwarding to services");
	kret = GSS_KRB5_S_G_BAD_USAGE;
	goto out;
    }

    kret = krb5_get_forwarded_creds(context,
				    ac,
				    ccache,
				    KDCOptions2int(fwd_flags),
				    name->name.name_string.val[1],
				    &creds,
				    fwd_data);
    if (kret)
	goto out;

 out:
    _gss_mg_log(1, "gss-krb5: delegation %s -> %s",
		(flagmask & GSS_C_DELEG_POLICY_FLAG) ?
		"ok-as-delegate" : "delegate",
		fwd_data->length ? "yes" : "no");

    if (kret)
	*flags &= ~flagmask;
    else
	*flags |= flagmask;

    if (creds.client)
	krb5_free_principal(context, creds.client);
    if (creds.server)
	krb5_free_principal(context, creds.server);
}


static OM_uint32
setup_icc(krb5_context context,
	  gsskrb5_ctx ctx,
	  krb5_principal client)
{
    krb5_error_code ret;

    heim_assert(ctx->gic_opt == NULL, "icc already setup");

    _gss_mg_log(1, "gss-iakerb: setup_icc: cert: %s passwd: %s",
		ctx->cert ? "yes" : "no",
		ctx->password ? "yes" : "no");


    ret = krb5_get_init_creds_opt_alloc(context, &ctx->gic_opt);
    if (ret)
	return ret;

    krb5_get_init_creds_opt_set_canonicalize(context, ctx->gic_opt, TRUE);

#ifdef PKINIT
    if (ctx->cert) {
	char *cert_pool[2] = { "KEYCHAIN:", NULL };

	ret = krb5_get_init_creds_opt_set_pkinit(context, ctx->gic_opt, client,
						 NULL, "KEYCHAIN:", 
						 cert_pool, NULL, 8,
						 NULL, NULL, NULL);
	if (ret)
	    return ret;
    }
#endif

    ret = krb5_init_creds_init(context, client, NULL, NULL, 0,
			       ctx->gic_opt, &ctx->asctx);
    if (ret)
	return ret;


#ifndef PKINIT
    heim_assert(ctx->password, "no password");
#else
    heim_assert(ctx->password || ctx->cert, "no password nor cert ?");

    if (ctx->cert) {
	ret = krb5_init_creds_set_pkinit_client_cert(context, ctx->asctx, ctx->cert);
	if (ret)
	    return ret;
    }
#endif
    if (ctx->password) {
	ret = krb5_init_creds_set_password(context, ctx->asctx, ctx->password);
	if (ret)
	    return ret;
    }

    return 0;
}

#ifdef PKINIT

static OM_uint32
init_pku2u_auth(OM_uint32 * minor_status,
		gsskrb5_cred cred,
		gsskrb5_ctx ctx,
		krb5_context context,
		gss_name_t name,
		const gss_OID mech_type,
		OM_uint32 req_flags,
		OM_uint32 time_req,
		gss_channel_bindings_t channel_bindings,
		const gss_buffer_t input_token,
		gss_buffer_t output_token,
		OM_uint32 * ret_flags,
		OM_uint32 * time_rec)
{
    OM_uint32 maj_stat = GSS_S_FAILURE;
    krb5_error_code ret;
    krb5_principal client = NULL;

    *minor_status = 0;

    ctx->messages = krb5_storage_emem();
    if (ctx->messages == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    /*
     * XXX Search for existing credentials here before going of and
     * doing PK-U2U
     */


    /*
     * Pick upp the certificate from gsskrb5_cred.
     * XXX fix better mapping for client principal.
     */

    if (cred == NULL) {
	gss_cred_id_t temp;
	gsskrb5_cred cred2;

	maj_stat = _gsspku2u_acquire_cred(minor_status, GSS_C_NO_NAME,
					  GSS_C_INDEFINITE, GSS_C_NO_OID_SET,
					  GSS_C_INITIATE, &temp, NULL, NULL);
	if (maj_stat)
	    return maj_stat;
					  
	cred2 = (gsskrb5_cred)temp;

	ret = krb5_copy_principal(context, cred2->principal, &client);
	if (ret) {
	    _gsskrb5_release_cred(minor_status, &temp);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ctx->cert = hx509_cert_ref(cred2->cert);
	_gsskrb5_release_cred(minor_status, &temp);

	maj_stat = GSS_S_FAILURE;
    } else if (cred->cert) {
	ret = krb5_copy_principal(context, cred->principal, &client);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	ctx->cert = hx509_cert_ref(cred->cert);

    } else {
	*minor_status = EINVAL;
	return GSS_S_FAILURE;
    }

    ret = setup_icc(context, ctx, client);
    if (ret) {
	*minor_status = ret;
	goto out;
    }

    /* XXX should be based on target_name */
    ret = krb5_init_creds_set_service(context, ctx->asctx, "WELLKNOWN/NULL");
    if (ret) {
	*minor_status = ret;
	goto out;
    }

    if (krb5_principal_is_null(context, client)) {
	InitiatorNameAssertion na;
	hx509_name subject;
	krb5_data data;
	size_t size = 0;
	Name n;

	memset(&na, 0, sizeof(na));
	memset(&n, 0, sizeof(n));

	na.initiatorName = calloc(1, sizeof(*na.initiatorName));
	if (na.initiatorName == NULL) {
	    *minor_status = ENOMEM;
	    goto out;
	}
	
	ret = hx509_cert_get_subject(ctx->cert, &subject);
	if (ret) {
	    free_InitiatorNameAssertion(&na);
	    *minor_status = ret;
	    goto out;
	}

	ret = hx509_name_to_Name(subject, &n);
	hx509_name_free(&subject);
	if (ret) {
	    free_InitiatorNameAssertion(&na);
	    *minor_status = ret;
	    goto out;
	}

	na.initiatorName->element =
	    choice_InitiatorName_nameNotInCert;
	na.initiatorName->u.nameNotInCert.element =
	    choice_GeneralName_directoryName;
	na.initiatorName->u.nameNotInCert.u.directoryName.element =
	    choice_GeneralName_directoryName_rdnSequence;
	na.initiatorName->u.nameNotInCert.u.directoryName.u.rdnSequence.len =
	    n.u.rdnSequence.len;
	na.initiatorName->u.nameNotInCert.u.directoryName.u.rdnSequence.val =
	    n.u.rdnSequence.val;

	ASN1_MALLOC_ENCODE(InitiatorNameAssertion, data.data, data.length,
			   &na, &size, ret);
	free_InitiatorNameAssertion(&na);
	if (ret)
	    goto out;
	if (size != data.length)
	    krb5_abortx(context, "internal error in ASN.1 encoder");

	ret = _krb5_init_creds_set_pku2u(context, ctx->asctx, &data);
	krb5_data_free(&data);
    } else {
	ret = _krb5_init_creds_set_pku2u(context, ctx->asctx, NULL);
    }

    if (ret) {
	*minor_status = ret;
	goto out;
    }

    maj_stat = GSS_S_COMPLETE;
    ctx->initiator_state = step_pku2u_auth;
 out:
    if (client)
	krb5_free_principal(context, client);

    return maj_stat;
}

static OM_uint32
step_pku2u_auth(OM_uint32 * minor_status,
		gsskrb5_cred cred,
		gsskrb5_ctx ctx,
		krb5_context context,
		gss_name_t name,
		const gss_OID mech_type,
		OM_uint32 req_flags,
		OM_uint32 time_req,
		gss_channel_bindings_t channel_bindings,
		const gss_buffer_t input_token,
		gss_buffer_t output_token,
		OM_uint32 * ret_flags,
		OM_uint32 * time_rec)
{
    OM_uint32 maj_stat;
    unsigned int flags = 0;
    krb5_error_code ret;
    krb5_data in, out;

    krb5_data_zero(&out);

    if (input_token && input_token->length) {

	krb5_storage_write(ctx->messages,
			   input_token->value,
			   input_token->length);

	maj_stat = _gsskrb5_decapsulate (minor_status,
					 input_token,
					 &in,
					 "\x06\x00",
					 ctx->mech);
	if (maj_stat)
	    return maj_stat;
    } else
	krb5_data_zero(&in);

    maj_stat = GSS_S_FAILURE;

    ret = krb5_init_creds_step(context, ctx->asctx, 
			       &in, &out, NULL, NULL, &flags);
    if (ret)
	goto out;

    if ((flags & 1) == 0) {

	ctx->kcred = calloc(1, sizeof(*ctx->kcred));
	if (ctx->kcred == NULL) {
	    ret = ENOMEM;
	    goto out;
	}

	/*
	 * Pull out credential and store in ctx->kcred and just use
	 * that as the credential in AP-REQ.
	 */

	ret = krb5_init_creds_get_creds(context, ctx->asctx, ctx->kcred);
	krb5_init_creds_free(context, ctx->asctx);
	ctx->asctx = NULL;
	if (ret)
	    goto out;

	ret = krb5_copy_principal(context, ctx->kcred->client, &ctx->source);
	if (ret)
	    goto out;
	ret = krb5_copy_principal(context, ctx->kcred->server, &ctx->target);
	if (ret)
	    goto out;

	/* XXX store credential in credential cache */
#if 0
	{
	    krb5_ccache id = NULL;

	    ret = krb5_cc_new_unique(context, "API", NULL, &id);
	    if (ret == 0)
		ret = krb5_cc_initialize(context, id, ctx->kcred->client);
	    if (ret == 0)
		krb5_cc_store_cred(context, id, ctx->kcred);
	    if (id)
		krb5_cc_close(id);
	}
#endif

	maj_stat = GSS_S_COMPLETE;
	ctx->initiator_state = step_setup_keys;

    } else {
        ret = _gsskrb5_encapsulate (minor_status, &out, output_token,
				    (u_char *)"\x05\x00", ctx->mech);
	if (ret)
	    goto out;

	krb5_storage_write(ctx->messages,
			   output_token->value,
			   output_token->length);

	maj_stat = GSS_S_CONTINUE_NEEDED;
    }

 out:
    *minor_status = ret;

    return maj_stat;
}

#endif /* PKINIT */


/*
 * IAKERB
 */

OM_uint32
_gsskrb5_iakerb_parse_header(OM_uint32 *minor_status,
			     krb5_context context,
			     gsskrb5_ctx ctx,
			     const gss_buffer_t input_token,
			     krb5_data *data)
{
    krb5_error_code ret;
    OM_uint32 maj_stat;
    uint8_t type[2];
    size_t size;
    
    maj_stat = _gssapi_decapsulate(minor_status, input_token, type, data, GSS_IAKERB_MECHANISM);
    if (maj_stat)
	return maj_stat;
    
    if (memcmp(type, "\x05\x01", 2) == 0) {
	IAKERB_HEADER header;
	
	ret = decode_IAKERB_HEADER(data->data, data->length, &header, &size);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	
	heim_assert(data->length >= size, "internal asn1 decoder failure");
	
	data->data = ((uint8_t *)data->data) + size;
	data->length -= size;
	
	if (header.cookie) {
	    if (ctx->cookie)
		krb5_free_data(context, ctx->cookie);
	    (void)krb5_copy_data(context, header.cookie, &ctx->cookie);
	}
	
	if (ctx->iakerbrealm)
	    free(ctx->iakerbrealm);
	ctx->iakerbrealm = strdup(header.target_realm);

	free_IAKERB_HEADER(&header);
	
	return GSS_S_COMPLETE;
	
    } else if (memcmp(type, "\x03\x00", 2) == 0) {
	/* XXX parse KRB-ERROR and set appropriate minor code */
	*minor_status = 0;
	return GSS_S_FAILURE;
    } else {
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }
}

OM_uint32
_gsskrb5_iakerb_make_header(OM_uint32 *minor_status,
			    krb5_context context,
			    gsskrb5_ctx ctx,
			    krb5_realm realm,
			    krb5_data *kdata,
			    gss_buffer_t output_token)
{
    IAKERB_HEADER header;
    unsigned char *data;
    size_t length, size = 0;
    OM_uint32 maj_stat;
    krb5_data iadata;
    int ret;

    header.target_realm = realm;
    header.cookie = ctx->cookie;
    
    ASN1_MALLOC_ENCODE(IAKERB_HEADER, data, length, &header, &size, ret);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    heim_assert(length == size, "internal asn1 encoder error");
    
    if (ctx->cookie) {
	krb5_free_data(context, ctx->cookie);
	ctx->cookie = NULL;
    }
    
    iadata.length = length + kdata->length;
    iadata.data = malloc(iadata.length);
    if (iadata.data == NULL) {
	free(data);
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    memcpy(iadata.data, data, length);
    memcpy(((uint8_t *)iadata.data) + length, kdata->data, kdata->length);
    free(data);
    
    maj_stat = _gsskrb5_encapsulate(minor_status, &iadata, output_token,
				    (u_char *)"\x05\x01", ctx->mech);
    free(iadata.data);
    
    return maj_stat;
}

static OM_uint32
init_iakerb_auth(OM_uint32 * minor_status,
		 gsskrb5_cred cred,
		 gsskrb5_ctx ctx,
		 krb5_context context,
		 gss_name_t target_name,
		 const gss_OID mech_type,
		 OM_uint32 req_flags,
		 OM_uint32 time_req,
		 gss_channel_bindings_t channel_bindings,
		 const gss_buffer_t input_token,
		 gss_buffer_t output_token,
		 OM_uint32 * ret_flags,
		 OM_uint32 * time_rec)
{
    krb5_error_code ret;
    
    ctx->messages = krb5_storage_emem();
    if (ctx->messages == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    
    /* 
     * XXX
     */

    if (cred == NULL)
	return GSS_S_FAILURE;

    ret = krb5_copy_principal(context, cred->principal, &ctx->source);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    
    /* XXX this kind of sucks, forcing referrals and then fixing up LKDC realm later */
    ret = krb5_copy_principal(context, (krb5_const_principal)target_name, &ctx->target);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    krb5_principal_set_realm(context, ctx->target, ctx->source->realm);
    
    if (cred->password) {

	ctx->password = strdup(cred->password);
	if (ctx->password == NULL) {
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}

#ifdef PKINIT
    } else if (cred->cert) {

	ctx->cert = heim_retain(cred->cert);
#endif
    } else if (cred->cred_flags & GSS_CF_IAKERB_RESOLVED) {
	/* all work done in auth_tgs */
    } else {

	*minor_status = EINVAL;
	return GSS_S_FAILURE;
    }
	
    ctx->ccache = cred->ccache;

    /* capture ctx->ccache */
    krb5_cc_get_config(context, ctx->ccache, NULL, "FriendlyName", &ctx->friendlyname);
    krb5_cc_get_config(context, ctx->ccache, NULL, "lkdc-hostname", &ctx->lkdchostname);

    if (cred->cred_flags & GSS_CF_IAKERB_RESOLVED) {
	ctx->initiator_state = step_iakerb_auth_tgs;
    } else {
	ctx->initiator_state = step_iakerb_auth_as;
    }

    *minor_status = 0;


    return GSS_S_COMPLETE;
}
    
static OM_uint32
step_iakerb_auth_as(OM_uint32 * minor_status,
		    gsskrb5_cred cred,
		    gsskrb5_ctx ctx,
		    krb5_context context,
		    gss_name_t name,
		    const gss_OID mech_type,
		    OM_uint32 req_flags,
		    OM_uint32 time_req,
		    gss_channel_bindings_t channel_bindings,
		    const gss_buffer_t input_token,
		    gss_buffer_t output_token,
		    OM_uint32 * ret_flags,
		    OM_uint32 * time_rec)
{
    OM_uint32 maj_stat;
    krb5_data in, out;
    krb5_realm realm = NULL;
    krb5_error_code ret;
    unsigned int flags = 0;
    
    if (ctx->asctx == NULL) {
	/* first packet */
	
	ret = setup_icc(context, ctx, ctx->source);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	krb5_data_zero(&in);
	
    } else {

	krb5_storage_write(ctx->messages,
			   input_token->value,
			   input_token->length);

	maj_stat = _gsskrb5_iakerb_parse_header(minor_status, context, ctx, input_token, &in);
	if (maj_stat)
	    return maj_stat;
    }

    ret = krb5_init_creds_step(context, ctx->asctx, &in, &out, NULL, &realm, &flags);
    if (ret) {
	_gss_mg_log(1, "gss-iakerb: init_creds_step: %d", ret);
	_gsskrb5_error_token(minor_status, ctx->mech, context, ret,
			     NULL, NULL, output_token);
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    
    if (flags & KRB5_INIT_CREDS_STEP_FLAG_CONTINUE) {
	
	heim_assert(realm != NULL, "krb5_init_creds_step return data w/o a realm");
	
	maj_stat = _gsskrb5_iakerb_make_header(minor_status, context, ctx, realm, &out, output_token);
	if (maj_stat)
	    return maj_stat;
	
	krb5_storage_write(ctx->messages,
			   output_token->value,
			   output_token->length);

	
	return GSS_S_CONTINUE_NEEDED;

    } else {
	krb5_creds kcred;

	memset(&kcred, 0, sizeof(kcred));
	
	_gss_mg_log(1, "gss-iakerb: going to state auth-tgs");

	heim_assert(out.length == 0, "output of AS-REQ not 0 when done");
	
	/* store credential in credential cache and lets continue the TGS REQ */
	ret = krb5_init_creds_get_creds(context, ctx->asctx, &kcred);
	if (ret) {
	    *minor_status = ret;
	    _gsskrb5_error_token(minor_status, ctx->mech, context, ret,
				 NULL, NULL, output_token);
	    return GSS_S_FAILURE;
	}

	//(void)krb5_cc_new_unique(context, "MEMORY", NULL, &ctx->ccache);
	(void)krb5_cc_initialize(context, ctx->ccache, kcred.client);
	(void)krb5_cc_store_cred(context, ctx->ccache, &kcred);
	if (ctx->password) {
	    krb5_data pw;
	    pw.data = ctx->password;
	    pw.length = strlen(ctx->password);
	    (void)krb5_cc_set_config(context, ctx->ccache, NULL, "password", &pw);
	}
#ifdef PKINIT
	if (ctx->cert) {
	    krb5_data pd;
	    ret = hx509_cert_get_persistent(ctx->cert, &pd);
	    if (ret) {
		*minor_status = ret;
		return GSS_S_FAILURE;
	    }
	    krb5_cc_set_config(context, ctx->ccache, NULL, "certificate-ref", &pd);
	    der_free_octet_string(&pd);
	}
#endif
	if (ctx->friendlyname.length)
	    krb5_cc_set_config(context, ctx->ccache, NULL, "FriendlyName", &ctx->friendlyname);
	if (ctx->lkdchostname.length) {
	    krb5_data data = { 1, "1" } ;
	    krb5_cc_set_config(context, ctx->ccache, NULL, "lkdc-hostname", &ctx->lkdchostname);
	    krb5_cc_set_config(context, ctx->ccache, NULL, "nah-created", &data);
	    krb5_cc_set_config(context, ctx->ccache, NULL, "iakerb", &data);
	}
	
	/* update source if we got a referrals */
	krb5_free_principal(context, ctx->source);
	(void)krb5_copy_principal(context, kcred.client, &ctx->source);
	
	krb5_free_cred_contents(context, &kcred);

	ctx->initiator_state = step_iakerb_auth_tgs;
	
	return GSS_S_COMPLETE;
    }
    
}

static OM_uint32
step_iakerb_auth_tgs(OM_uint32 * minor_status,
		     gsskrb5_cred cred,
		     gsskrb5_ctx ctx,
		     krb5_context context,
		     gss_name_t name,
		     const gss_OID mech_type,
		     OM_uint32 req_flags,
		     OM_uint32 time_req,
		     gss_channel_bindings_t channel_bindings,
		     const gss_buffer_t input_token,
		     gss_buffer_t output_token,
		     OM_uint32 * ret_flags,
		     OM_uint32 * time_rec)
{
    krb5_realm realm = NULL;
    OM_uint32 maj_stat;
    krb5_data in, out;
    krb5_error_code ret;
    unsigned int flags = 0;

    *minor_status = 0;
    krb5_data_zero(&out);
    
    if (ctx->tgsctx == NULL) {
	krb5_creds incred;
	
	memset(&incred, 0, sizeof(incred));
	
	incred.client = ctx->source;
	incred.server = ctx->target;
	
	ret = krb5_tkt_creds_init(context, ctx->ccache, &incred, 0, &ctx->tgsctx);
	if (ret) {
	    *minor_status = ret;
	    _gsskrb5_error_token(minor_status, ctx->mech, context, ret,
				 NULL, NULL, output_token);
	    return GSS_S_FAILURE;
	}
	
    } else {

	krb5_storage_write(ctx->messages,
			   input_token->value,
			   input_token->length);

	maj_stat = _gsskrb5_iakerb_parse_header(minor_status, context, ctx, input_token, &in);
	if (maj_stat)
	    return maj_stat;
    }
    
    ret = krb5_tkt_creds_step(context, ctx->tgsctx, &in, &out, &realm, &flags);
    if (ret) {
	_gss_mg_log(1, "gss-iakerb: tkt_creds_step: %d", ret);
	_gsskrb5_error_token(minor_status, ctx->mech, context, ret,
			     NULL, NULL, output_token);
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    if (flags & KRB5_TKT_STATE_CONTINUE) {
	
	maj_stat = _gsskrb5_iakerb_make_header(minor_status, context, ctx, realm, &out, output_token);
	krb5_data_free(&out);
	if (maj_stat != GSS_S_COMPLETE)
	    return maj_stat;
	
	krb5_storage_write(ctx->messages,
			   output_token->value,
			   output_token->length);

	return GSS_S_CONTINUE_NEEDED;

    } else {
	heim_assert(out.length == 0, "output data is not zero");
	
	_gss_mg_log(1, "gss-iakerb: going to state setup-keys");

	ret = krb5_tkt_creds_get_creds(context, ctx->tgsctx, &ctx->kcred);
	if (ret) {
	    _gss_mg_log(1, "gss-iakerb: tkt_get_creds: %d", ret);
	    _gsskrb5_error_token(minor_status, ctx->mech, context, ret,
				 NULL, NULL, output_token);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	
	ctx->endtime = ctx->kcred->times.endtime;

	ctx->initiator_state = step_setup_keys;
	
	return GSS_S_COMPLETE;
    }
}

/*
 * KRB5
 */

static OM_uint32
init_krb5_auth(OM_uint32 * minor_status,
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
	       OM_uint32 * time_rec)
{
    OM_uint32 ret = GSS_S_FAILURE;
    krb5_error_code kret;
    krb5_data outbuf;
    krb5_data fwd_data;
    OM_uint32 lifetime_rec;
    int allow_dns = 1;

    krb5_data_zero(&outbuf);
    krb5_data_zero(&fwd_data);

    *minor_status = 0;

    /*
     * If ctx->ccache is set, we already have a cache and source 
     */

    if (ctx->ccache == NULL) {

	if (cred == NULL) {
	    kret = krb5_cc_default (context, &ctx->ccache);
	    if (kret) {
		*minor_status = kret;
		ret = GSS_S_FAILURE;
		goto failure;
	    }
	    ctx->more_flags |= CLOSE_CCACHE;
	} else
	    ctx->ccache = cred->ccache;

	kret = krb5_cc_get_principal (context, ctx->ccache, &ctx->source);
	if (kret) {
	    *minor_status = kret;
	    ret = GSS_S_FAILURE;
	    goto failure;
	}
    }

    /*
     * This is hideous glue for (NFS) clients that wants to limit the
     * available enctypes to what it can support (encryption in
     * kernel). If there is no enctypes selected for this credential,
     * reset it to the default set of enctypes.
     */
    {
	krb5_enctype *enctypes = NULL;

	if (cred && cred->enctypes)
	    enctypes = cred->enctypes;
	krb5_set_default_in_tkt_etypes(context, enctypes);
    }

    /* canon name if needed for client + target realm */
    kret = krb5_cc_get_config(context, ctx->ccache, NULL,
			      "realm-config", &outbuf);
    if (kret == 0) {
	/* XXX 2 is no server canon */
	if (outbuf.length < 1 || ((((unsigned char *)outbuf.data)[0]) & 2))
	    allow_dns = 0;
	krb5_data_free(&outbuf);
    }

    if (_gss_mg_log_level(1)) {
	char *str, *fullname;
	ret = krb5_unparse_name(context, ctx->source, &str);
	if (ret)
	    goto failure;

	ret = krb5_cc_get_full_name(context, ctx->ccache, &fullname);
	if (ret) {
	    krb5_xfree(str);
	    goto failure;
	}
	_gss_mg_log(1, "gss-krb5: ISC client: %s cache: %s", str, fullname);

	krb5_xfree(str);
	krb5_xfree(fullname);
    }

    /*
     * First we try w/o dns, hope that the KDC have register alias
     * (and referrals if cross realm) for this principal. If that
     * fails and if we are allowed to using this realm try again with
     * DNS canonicalizion.
     */
    ret = gsskrb5_get_creds(minor_status, context, ctx->ccache,
			    ctx, name, 0, time_req,
			    time_rec);
    if (ret && allow_dns)
	ret = gsskrb5_get_creds(minor_status, context, ctx->ccache,
				ctx, name, 1, time_req,
				time_rec);
    if (ret)
	goto failure;

    ctx->endtime = ctx->kcred->times.endtime;

    ret = _gss_DES3_get_mic_compat(minor_status, ctx, context);
    if (ret)
	goto failure;

    ret = _gsskrb5_lifetime_left(minor_status,
				 context,
				 ctx->endtime,
				 &lifetime_rec);
    if (ret)
	goto failure;

    if (lifetime_rec == 0) {
	_gss_mg_log(1, "gss-krb5: credentials expired");
	*minor_status = 0;
	ret = GSS_S_CONTEXT_EXPIRED;
	goto failure;
    }

    ctx->initiator_state = step_setup_keys;

    return GSS_S_COMPLETE;

failure:

#ifdef __APPLE__
    if (GSS_ERROR(ret))
	update_neg_cache(ret, *minor_status, context,
			 ctx->mech, cred, (krb5_principal)name);
#endif

    return ret;
}

/*
 * Common part of iakerb, pku2u and krb5
 */

static OM_uint32
step_setup_keys(OM_uint32 * minor_status,
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
		OM_uint32 * time_rec)
{
    krb5_error_code kret;
    
    heim_assert(ctx->kcred != NULL, "gsskrb5 context is missing kerberos credential");

    krb5_auth_con_setkey(context, ctx->auth_context, &ctx->kcred->session);
    krb5_auth_con_setkey(context, ctx->deleg_auth_context, &ctx->kcred->session);

    kret = krb5_auth_con_generatelocalsubkey(context,
					     ctx->auth_context,
					     &ctx->kcred->session);
    if (kret) {
	*minor_status = kret;
	return GSS_S_FAILURE;
    }

    ctx->initiator_state = init_auth_step;

    return GSS_S_COMPLETE;
}


static OM_uint32
init_auth_step(OM_uint32 * minor_status,
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
	       OM_uint32 * time_rec)
{
    krb5_crypto crypto = NULL;
    OM_uint32 ret = GSS_S_FAILURE;
    krb5_error_code kret;
    krb5_flags ap_options;
    krb5_data outbuf;
    uint32_t flags;
    krb5_data authenticator;
    Checksum cksum;
    krb5_enctype enctype;

    krb5_data fwd_data, timedata, pkt_cksum;
    int32_t offset = 0, oldoffset = 0;
    uint32_t flagmask;

    krb5_data_zero(&outbuf);
    krb5_data_zero(&fwd_data);
    krb5_data_zero(&pkt_cksum);
    memset(&cksum, 0, sizeof(cksum));

    *minor_status = 0;

    /*
     * If the credential doesn't have ok-as-delegate, check if there
     * is a realm setting and use that.
     */
    if (!ctx->kcred->flags.b.ok_as_delegate && ctx->ccache) {
	krb5_data data;

	ret = krb5_cc_get_config(context, ctx->ccache, NULL,
				 "realm-config", &data);
	if (ret == 0) {
	    /* XXX 1 is use ok-as-delegate */
	    if (data.length < 1 || ((((unsigned char *)data.data)[0]) & 1) == 0)
		req_flags &= ~(GSS_C_DELEG_FLAG|GSS_C_DELEG_POLICY_FLAG);
	    krb5_data_free(&data);
	}
    }

    flagmask = 0;

    /* if we used GSS_C_DELEG_POLICY_FLAG, trust KDC */
    if ((req_flags & GSS_C_DELEG_POLICY_FLAG)
	&& ctx->kcred->flags.b.ok_as_delegate)
	flagmask |= GSS_C_DELEG_FLAG | GSS_C_DELEG_POLICY_FLAG;
    /* if there still is a GSS_C_DELEG_FLAG, use that */
    if (req_flags & GSS_C_DELEG_FLAG)
	flagmask |= GSS_C_DELEG_FLAG;


    flags = 0;
    ap_options = 0;
    if ((flagmask & GSS_C_DELEG_FLAG) && ctx->ccache) {
	do_delegation (context,
		       ctx->deleg_auth_context,
		       ctx->ccache, ctx->kcred, ctx->target,
		       &fwd_data, flagmask, &flags);
    }

    if (req_flags & GSS_C_MUTUAL_FLAG) {
	flags |= GSS_C_MUTUAL_FLAG;
	ap_options |= AP_OPTS_MUTUAL_REQUIRED;
    }

    if (req_flags & GSS_C_REPLAY_FLAG)
	flags |= GSS_C_REPLAY_FLAG;
    if (req_flags & GSS_C_SEQUENCE_FLAG)
	flags |= GSS_C_SEQUENCE_FLAG;
#if 0
    if (req_flags & GSS_C_ANON_FLAG)
	;                               /* XXX */
#endif
    if (req_flags & GSS_C_DCE_STYLE) {
	/* GSS_C_DCE_STYLE implies GSS_C_MUTUAL_FLAG */
	flags |= GSS_C_DCE_STYLE | GSS_C_MUTUAL_FLAG;
	ap_options |= AP_OPTS_MUTUAL_REQUIRED;
    }
    if (req_flags & GSS_C_IDENTIFY_FLAG)
	flags |= GSS_C_IDENTIFY_FLAG;
    if (req_flags & GSS_C_EXTENDED_ERROR_FLAG)
	flags |= GSS_C_EXTENDED_ERROR_FLAG;

    if (req_flags & GSS_C_CONF_FLAG) {
	flags |= GSS_C_CONF_FLAG;
    }
    if (req_flags & GSS_C_INTEG_FLAG) {
	flags |= GSS_C_INTEG_FLAG;
    }
    if (cred == NULL || !(cred->cred_flags & GSS_CF_NO_CI_FLAGS)) {
	flags |= GSS_C_CONF_FLAG;
	flags |= GSS_C_INTEG_FLAG;
    }
    flags |= GSS_C_TRANS_FLAG;

    ctx->flags = flags;
    ctx->more_flags |= LOCAL;

    ret = krb5_crypto_init(context, ctx->auth_context->local_subkey,
			   0, &crypto);
    if (ret) {
	goto failure;
    }

    if (ctx->messages) {
	GSS_KRB5_FINISHED pkt;
	krb5_data pkts;
	size_t size = 0;
	
	memset(&pkt, 0, sizeof(pkt));

	ret = krb5_storage_to_data(ctx->messages, &pkts);
	krb5_storage_free(ctx->messages);
	ctx->messages = NULL;
	if (ret)
	    goto failure;

	ret = krb5_create_checksum(context,
				   crypto,
				   KRB5_KU_FINISHED,
				   0,
				   pkts.data,
				   pkts.length,
				   &pkt.gss_mic);
	krb5_data_free(&pkts);
	if (ret)
	    goto failure;

	ASN1_MALLOC_ENCODE(GSS_KRB5_FINISHED,
			   pkt_cksum.data, pkt_cksum.length,
			   &pkt, &size, ret);
	free_GSS_KRB5_FINISHED(&pkt);
	if (ret)
	    goto failure;
	if (pkt_cksum.length != size)
	    krb5_abortx(context, "internal error in ASN.1 encoder");
    }

    ret = _gsskrb5_create_8003_checksum (minor_status,
					 context,
					 crypto,
					 input_chan_bindings,
					 flags,
					 &fwd_data,
					 &pkt_cksum,
					 &cksum);
    if (ret)
	goto failure;

    enctype = ctx->auth_context->keyblock->keytype;

    if (ctx->ccache) {

	ret = krb5_cc_get_config(context, ctx->ccache, ctx->target,
				 "time-offset", &timedata);
	if (ret == 0) {
	    if (timedata.length == 4) {
		const u_char *p = timedata.data;
		offset = (p[0] <<24) | (p[1] << 16) | (p[2] << 8) | (p[3] << 0);
	    }
	    krb5_data_free(&timedata);
	}

	if (offset) {
	    krb5_get_kdc_sec_offset(context, &oldoffset, NULL);
	    krb5_set_kdc_sec_offset(context, offset, -1);
	}
    }

    kret = _krb5_build_authenticator(context,
				     ctx->auth_context,
				     enctype,
				     ctx->kcred,
				     &cksum,
				     &authenticator,
				     KRB5_KU_AP_REQ_AUTH);

    if (kret) {
	if (offset)
	    krb5_set_kdc_sec_offset (context, oldoffset, -1);
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    kret = krb5_build_ap_req (context,
			      enctype,
			      ctx->kcred,
			      ap_options,
			      authenticator,
			      &outbuf);
    if (offset)
	krb5_set_kdc_sec_offset (context, oldoffset, -1);
    if (kret) {
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    if (flags & GSS_C_DCE_STYLE) {
	output_token->value = outbuf.data;
	output_token->length = outbuf.length;
    } else {
        ret = _gsskrb5_encapsulate (minor_status, &outbuf, output_token,
				    (u_char *)(intptr_t)"\x01\x00", ctx->mech);
	krb5_data_free (&outbuf);
	if (ret)
	    goto failure;
    }

    if (crypto)
	krb5_crypto_destroy(context, crypto);
    free_Checksum(&cksum);
    krb5_data_free (&fwd_data);
    krb5_data_free (&pkt_cksum);

    if (flags & GSS_C_MUTUAL_FLAG) {
	ctx->initiator_state = wait_repl_mutual;
	return GSS_S_CONTINUE_NEEDED;
    }

    return initiator_ready(minor_status, ctx, context, ret_flags);

failure:
    if (crypto)
	krb5_crypto_destroy(context, crypto);
    free_Checksum(&cksum);
    krb5_data_free (&fwd_data);
    krb5_data_free (&pkt_cksum);
    return ret;
}

static OM_uint32
handle_error_packet(OM_uint32 *minor_status,
		    krb5_context context,
		    gsskrb5_ctx ctx,
		    krb5_data indata)
{
    krb5_error_code kret;
    KRB_ERROR error;

    heim_assert(ctx->initiator_state == wait_repl_mutual,
		"handle_error in wrong state");

    kret = krb5_rd_error(context, &indata, &error);
    if (kret == 0) {
	kret = krb5_error_from_rd_error(context, &error, NULL);

	_gss_mg_log(1, "gss-krb5: server return KRB-ERROR with error code %d", kret);

	/*
	 * If we get back an error code that we know about, then lets retry again:
	 * - KRB5KRB_AP_ERR_MODIFIED: delete entry and retry
	 * - KRB5KRB_AP_ERR_SKEW: time skew sent, retry again with right time skew
	 * - KRB5KRB_ERR_GENERIC: with edata, maybe get windows error code or time skew (windows style)
	 */

	if (kret == KRB5KRB_AP_ERR_MODIFIED && ctx->ccache) {

	    if ((ctx->more_flags & RETRIED_NEWTICKET) == 0) {
		krb5_creds mcreds; 
                       
		_gss_mg_log(1, "gss-krb5: trying to renew ticket");

		krb5_cc_clear_mcred(&mcreds);
		mcreds.client = ctx->source;
		mcreds.server = ctx->target;
                       
		krb5_cc_remove_cred(context, ctx->ccache, 0, &mcreds);

		ctx->initiator_state = init_krb5_auth;
	    }
	    ctx->more_flags |= RETRIED_NEWTICKET;

	} else if (kret == KRB5KRB_AP_ERR_SKEW && ctx->ccache) {

	recover_skew:
	    if ((ctx->more_flags & RETRIED_SKEW) == 0) {
		krb5_data timedata;
		uint8_t p[4];
		int32_t t = (int32_t)(error.stime - time(NULL));

		_gss_mg_encode_be_uint32(t, p);

		timedata.data = p;
		timedata.length = sizeof(p);

		krb5_cc_set_config(context, ctx->ccache, ctx->target,
				   "time-offset", &timedata);
		
		_gss_mg_log(1, "gss-krb5: time skew of %d", (int)t);

		ctx->initiator_state = init_auth_step;
	    }
	    ctx->more_flags |= RETRIED_SKEW;
	} else if (kret == KRB5KRB_ERR_GENERIC && error.e_data) {
	    KERB_ERROR_DATA data;
	    krb5_error_code ret;

	    _gss_mg_log(1, "gss-krb5: trying to decode a KRB5KRB_ERR_GENERIC");

	    ret = decode_KERB_ERROR_DATA(error.e_data->data, error.e_data->length, &data, NULL);
	    if (ret)
		goto out;
				   
	    if (data.data_type == KRB5_AP_ERR_WINDOWS_ERROR_CODE) {
		if (data.data_value && data.data_value->length >= 4) {
		    uint32_t num;
		    _gss_mg_decode_le_uint32(data.data_value->data, &num);
		    _gss_mg_log(1, "gss-krb5: got an windows error code: %08x", (unsigned int)num);
		}
	    } else if (data.data_type == KRB5_AP_ERR_TYPE_SKEW_RECOVERY) {
		free_KERB_ERROR_DATA(&data);
		goto recover_skew;
	    } else {
		_gss_mg_log(1, "gss-krb5: got an KERB_ERROR_DATA of type %d", data.data_type);
	    }
	    free_KERB_ERROR_DATA(&data);
	}
	free_KRB_ERROR (&error);
    }

    if (ctx->initiator_state != wait_repl_mutual)
	return GSS_S_COMPLETE;

 out:
    *minor_status = kret;
    return GSS_S_FAILURE;

}


static OM_uint32
wait_repl_mutual(OM_uint32 * minor_status,
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
		 OM_uint32 * time_rec)
{
    OM_uint32 ret;
    krb5_error_code kret;
    krb5_data indata;
    krb5_ap_rep_enc_part *repl;

    output_token->length = 0;
    output_token->value = NULL;

    if (IS_DCE_STYLE(ctx)) {
	/* There is no OID wrapping. */
	indata.length	= input_token->length;
	indata.data	= input_token->value;
	kret = krb5_rd_rep(context,
			   ctx->auth_context,
			   &indata,
			   &repl);
	if (kret) {
	    ret = _gsskrb5_decapsulate(minor_status,
				       input_token,
				       &indata,
				       "\x03\x00",
				       GSS_KRB5_MECHANISM);
	    if (ret == GSS_S_COMPLETE) {
		return handle_error_packet(minor_status, context, ctx, indata);
	    } else {
		*minor_status = kret;
		return GSS_S_FAILURE;
	    }
	}
    } else {
	ret = _gsskrb5_decapsulate (minor_status,
				    input_token,
				    &indata,
				    "\x02\x00",
				    ctx->mech);
	if (ret == GSS_S_DEFECTIVE_TOKEN) {
	    /* check if there is an error token sent instead */
	    ret = _gsskrb5_decapsulate (minor_status,
					input_token,
					&indata,
					"\x03\x00",
					ctx->mech);
	    if (ret != GSS_S_COMPLETE)
		return ret;

	    return handle_error_packet(minor_status, context, ctx, indata);
	} else if (ret != GSS_S_COMPLETE) {
	    return ret;
	}

	kret = krb5_rd_rep (context,
			    ctx->auth_context,
			    &indata,
			    &repl);
	if (kret) {
	    *minor_status = kret;
	    return GSS_S_FAILURE;
	}
    }

    krb5_free_ap_rep_enc_part (context,
			       repl);

    *minor_status = 0;
    if (time_rec) {
	ret = _gsskrb5_lifetime_left(minor_status,
				     context,
				     ctx->endtime,
				     time_rec);
	if (ret)
	    return ret;
    }

    if (req_flags & GSS_C_DCE_STYLE) {
	int32_t local_seq, remote_seq;
	krb5_data outbuf;

	/*
	 * So DCE_STYLE is strange. The client echos the seq number
	 * that the server used in the server's mk_rep in its own
	 * mk_rep(). After when done, it resets to it's own seq number
	 * for the gss_wrap calls.
	 */

	krb5_auth_con_getremoteseqnumber(context, ctx->auth_context, &remote_seq);
	krb5_auth_con_getlocalseqnumber(context, ctx->auth_context, &local_seq);
	krb5_auth_con_setlocalseqnumber(context, ctx->auth_context, remote_seq);

	kret = krb5_mk_rep(context, ctx->auth_context, &outbuf);
	if (kret) {
	    *minor_status = kret;
	    return GSS_S_FAILURE;
	}

	/* reset local seq number */
	krb5_auth_con_setlocalseqnumber(context, ctx->auth_context, local_seq);

	output_token->length = outbuf.length;
	output_token->value  = outbuf.data;
    }

    return initiator_ready(minor_status, ctx, context, ret_flags);
}

static OM_uint32
step_completed(OM_uint32 * minor_status,
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
	       OM_uint32 * time_rec)
{
    /*
     * If we get there, the caller have called
     * gss_init_sec_context() one time too many.
     */
    _gsskrb5_set_status(EINVAL, "init_sec_context "
			"called one time too many");
    *minor_status = EINVAL;
    return GSS_S_BAD_STATUS;
}

/*
 * gss_init_sec_context
 */

OM_uint32 GSSAPI_CALLCONV _gsskrb5_init_sec_context
(OM_uint32 * minor_status,
 const gss_cred_id_t cred_handle,
 gss_ctx_id_t * context_handle,
 const gss_name_t target_name,
 const gss_OID mech_type,
 OM_uint32 req_flags,
 OM_uint32 time_req,
 const gss_channel_bindings_t input_chan_bindings,
 const gss_buffer_t input_token,
 gss_OID * actual_mech_type,
 gss_buffer_t output_token,
 OM_uint32 * ret_flags,
 OM_uint32 * time_rec
    )
{
    krb5_context context;
    gsskrb5_cred cred = (gsskrb5_cred)cred_handle;
    gsskrb5_ctx ctx;
    OM_uint32 ret;
    gss_OID mech = GSS_C_NO_OID;
    gsskrb5_initator_state start_state;

    GSSAPI_KRB5_INIT (&context);

    output_token->length = 0;
    output_token->value  = NULL;

    if (context_handle == NULL) {
	*minor_status = 0;
	return GSS_S_FAILURE | GSS_S_CALL_BAD_STRUCTURE;
    }

    if (ret_flags)
	*ret_flags = 0;
    if (time_rec)
	*time_rec = 0;

    if (target_name == GSS_C_NO_NAME) {
	if (actual_mech_type)
	    *actual_mech_type = GSS_C_NO_OID;
	*minor_status = 0;
	return GSS_S_BAD_NAME;
    }

    if (cred && (cred->usage != GSS_C_INITIATE && cred->usage != GSS_C_BOTH)) {
	krb5_set_error_message(context, GSS_KRB5_S_G_BAD_USAGE,
			       "ISC: Credentials not of "
			       "usage type initiator or both");
	*minor_status = GSS_KRB5_S_G_BAD_USAGE;
	return GSS_S_DEFECTIVE_CREDENTIAL;
    }

    if (gss_oid_equal(mech_type, GSS_KRB5_MECHANISM)) {
	mech = GSS_KRB5_MECHANISM;
	start_state = init_krb5_auth;
    } else if (gss_oid_equal(mech_type, GSS_IAKERB_MECHANISM)) {
	mech = GSS_IAKERB_MECHANISM;
	start_state = init_iakerb_auth;
#ifdef PKINIT
    } else if (gss_oid_equal(mech_type, GSS_PKU2U_MECHANISM)) {
	mech = GSS_PKU2U_MECHANISM;
	start_state = init_pku2u_auth;
#endif
    } else
	return GSS_S_BAD_MECH;

    if (input_token == GSS_C_NO_BUFFER || input_token->length == 0) {
#ifdef __APPLE__
        ret = check_neg_cache(minor_status, context, mech,
			      cred_handle, target_name);
        if (ret != GSS_S_COMPLETE)
            return ret;
#endif /* __APPLE__ */

	if (*context_handle != GSS_C_NO_CONTEXT) {
	    *minor_status = 0;
	    return GSS_S_FAILURE | GSS_S_CALL_BAD_STRUCTURE;
	}

	ret = _gsskrb5_create_ctx(minor_status,
				  context_handle,
				  context,
				  input_chan_bindings,
				  mech);
	if (ret)
	    return ret;
	
	ctx = (gsskrb5_ctx) *context_handle;
	ctx->initiator_state = start_state;
    } else {
	ctx = (gsskrb5_ctx) *context_handle;
    }

    if (ctx == NULL) {
	*minor_status = 0;
	return GSS_S_FAILURE | GSS_S_CALL_BAD_STRUCTURE;
    }

    HEIMDAL_MUTEX_lock(&ctx->ctx_id_mutex);

    if (actual_mech_type)
	*actual_mech_type = ctx->mech;

    do {
	ret = ctx->initiator_state(minor_status, cred, ctx, context, target_name,
				   mech, req_flags, time_req, input_chan_bindings, input_token,
				   output_token, ret_flags, time_rec);

    } while (ret == GSS_S_COMPLETE &&
	     ctx->initiator_state != step_completed &&
	     output_token->length == 0);

    if (GSS_ERROR(ret)) { 
	if (ctx->ccache && (ctx->more_flags & CLOSE_CCACHE))
	    krb5_cc_close(context, ctx->ccache);
	ctx->ccache = NULL;
    }

    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);

    /* destroy context in case of error */
    if (GSS_ERROR(ret)) {
	OM_uint32 junk;
	_gsskrb5_delete_sec_context(&junk, context_handle, GSS_C_NO_BUFFER);
    }

    return ret;

}
