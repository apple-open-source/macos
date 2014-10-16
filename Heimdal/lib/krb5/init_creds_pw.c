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

#include "krb5_locl.h"
#include <pkinit_asn1.h>

struct pa_info_data {
    krb5_enctype etype;
    krb5_salt salt;
    krb5_data *s2kparams;
};

struct krb5_init_creds_context_data {
    KDCOptions flags;
    krb5_creds cred;
    krb5_addresses *addrs;
    krb5_enctype *etypes;
    krb5_preauthtype *pre_auth_types;
    char *in_tkt_service;
    unsigned nonce;
    unsigned pk_nonce;

    krb5_data req_buffer;
    AS_REQ as_req;
    int pa_counter;

    /* password and keytab_data is freed on completion */
    char *password;
    krb5_keytab_key_proc_args *keytab_data;

    krb5_pointer *keyseed;
    krb5_s2k_proc keyproc;

    krb5_get_init_creds_tristate req_pac;

    krb5_pk_init_ctx pk_init_ctx;
    int ic_flags;

    char *kdc_hostname;

    int used_pa_types;

#define  USED_ENC_TS_INFO	8
#define  USED_ENC_TS_RENEG	16
    struct {
	unsigned int change_password:1;
	unsigned int allow_enc_pa_rep:1;
    } runflags;
    
    struct pa_info_data paid;

    METHOD_DATA md;
    KRB_ERROR error;
    AS_REP as_rep;
    EncKDCRepPart enc_part;

    krb5_prompter_fct prompter;
    void *prompter_data;
    int warned_user;

    struct pa_info_data *ppaid;

    struct krb5_fast_state fast_state;

    /* current and available pa mechansm in this exchange */
    struct pa_auth_mech *pa_mech;
    heim_array_t available_pa_mechs;

#ifdef PKINIT
    hx509_cert client_cert;
    krb5_data *pku2u_assertion;
#endif
    
    struct {
	struct timeval run_time;
    } stats;
};

static void
free_paid(krb5_context context, struct pa_info_data *ppaid)
{
    krb5_free_salt(context, ppaid->salt);
    if (ppaid->s2kparams)
	krb5_free_data(context, ppaid->s2kparams);
    memset(ppaid, 0, sizeof(*ppaid));
}

static krb5_error_code KRB5_CALLCONV
default_s2k_func(krb5_context context, krb5_enctype type,
		 krb5_const_pointer keyseed,
		 krb5_salt salt, krb5_data *s2kparms,
		 krb5_keyblock **key)
{
    krb5_error_code ret;
    krb5_data password;
    krb5_data opaque;

    if (_krb5_have_debug(context, 5)) {
	char *str = NULL;
	ret = krb5_enctype_to_string(context, type, &str);
	if (ret)
	    return ret;

	_krb5_debugx(context, 5, "krb5_get_init_creds: using default_s2k_func: %s (%d)", str, (int)type);
	free(str);
    }

    password.data = rk_UNCONST(keyseed);
    password.length = strlen(keyseed);
    if (s2kparms)
	opaque = *s2kparms;
    else
	krb5_data_zero(&opaque);

    *key = malloc(sizeof(**key));
    if (*key == NULL)
	return ENOMEM;
    ret = krb5_string_to_key_data_salt_opaque(context, type, password,
					      salt, opaque, *key);
    if (ret) {
	free(*key);
	*key = NULL;
    }
    return ret;
}

static void
free_init_creds_ctx(krb5_context context, krb5_init_creds_context ctx)
{
    if (ctx->etypes)
	free(ctx->etypes);
    if (ctx->pre_auth_types)
	free (ctx->pre_auth_types);
    if (ctx->in_tkt_service)
	free(ctx->in_tkt_service);
    if (ctx->keytab_data)
	free(ctx->keytab_data);
    if (ctx->password) {
	memset(ctx->password, 0, strlen(ctx->password));
	free(ctx->password);
    }
    /*
     * FAST state 
     */
    _krb5_fast_free(context, &ctx->fast_state);

    krb5_data_free(&ctx->req_buffer);
    krb5_free_cred_contents(context, &ctx->cred);
    free_METHOD_DATA(&ctx->md);
    free_AS_REP(&ctx->as_rep);
    free_EncKDCRepPart(&ctx->enc_part);
    free_KRB_ERROR(&ctx->error);
    free_AS_REQ(&ctx->as_req);

    heim_release(ctx->available_pa_mechs);
#ifdef PKINIT
    if (ctx->client_cert)
	hx509_cert_free(ctx->client_cert);
    if (ctx->pku2u_assertion)
	krb5_free_data(context, ctx->pku2u_assertion);
#endif
    if (ctx->kdc_hostname)
	free(ctx->kdc_hostname);
    free_paid(context, &ctx->paid);
    memset(ctx, 0, sizeof(*ctx));
}

static krb5_deltat
get_config_time (krb5_context context,
		 const char *realm,
		 const char *name,
		 int def)
{
    krb5_deltat ret;

    ret = krb5_config_get_time (context, NULL,
				"realms",
				realm,
				name,
				NULL);
    if (ret >= 0)
	return ret;
    ret = krb5_config_get_time (context, NULL,
				"libdefaults",
				name,
				NULL);
    if (ret >= 0)
	return ret;
    return def;
}

static krb5_error_code
init_cred (krb5_context context,
	   krb5_creds *cred,
	   krb5_principal client,
	   krb5_deltat start_time,
	   krb5_get_init_creds_opt *options)
{
    krb5_error_code ret;
    krb5_deltat tmp;
    krb5_timestamp now;

    krb5_timeofday (context, &now);

    memset (cred, 0, sizeof(*cred));

    if (client)
	krb5_copy_principal(context, client, &cred->client);
    else {
	ret = krb5_get_default_principal (context,
					  &cred->client);
	if (ret)
	    goto out;
    }

    if (start_time)
	cred->times.starttime  = now + start_time;

    if (options->flags & KRB5_GET_INIT_CREDS_OPT_TKT_LIFE)
	tmp = options->tkt_life;
    else
	tmp = 10 * 60 * 60;
    cred->times.endtime = now + tmp;

    if ((options->flags & KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE) &&
	options->renew_life > 0) {
	cred->times.renew_till = now + options->renew_life;
    }

    return 0;

out:
    krb5_free_cred_contents (context, cred);
    return ret;
}

/*
 * Print a message (str) to the user about the expiration in `lr'
 */

static void
report_expiration (krb5_context context,
		   krb5_prompter_fct prompter,
		   krb5_data *data,
		   const char *str,
		   time_t now)
{
    char *p = NULL;

    if (asprintf(&p, "%s%s", str, ctime(&now)) < 0 || p == NULL)
	return;
    (*prompter)(context, data, NULL, p, 0, NULL);
    free(p);
}

/*
 * Check the context, and in the case there is a expiration warning,
 * use the prompter to print the warning.
 *
 * @param context A Kerberos 5 context.
 * @param options An GIC options structure
 * @param ctx The krb5_init_creds_context check for expiration.
 */

krb5_error_code
krb5_process_last_request(krb5_context context,
			  krb5_get_init_creds_opt *options,
			  krb5_init_creds_context ctx)
{
    LastReq *lr;
    size_t i;

    /*
     * First check if there is a API consumer.
     */

    lr = &ctx->enc_part.last_req;

    if (options && options->opt_private && options->opt_private->lr.func) {
	krb5_last_req_entry **lre;

	lre = calloc(lr->len + 1, sizeof(*lre));
	if (lre == NULL) {
	    krb5_set_error_message(context, ENOMEM,
				   N_("malloc: out of memory", ""));
	    return ENOMEM;
	}
	for (i = 0; i < lr->len; i++) {
	    lre[i] = calloc(1, sizeof(*lre[i]));
	    if (lre[i] == NULL)
		break;
	    lre[i]->lr_type = lr->val[i].lr_type;
	    lre[i]->value = lr->val[i].lr_value;
	}

	(*options->opt_private->lr.func)(context, lre,
					 options->opt_private->lr.ctx);

	for (i = 0; i < lr->len; i++)
	    free(lre[i]);
	free(lre);
    }

    return krb5_init_creds_warn_user(context, ctx);
}

/**
 * Warn the user using prompter in the krb5_init_creds_context about
 * possible password and account expiration.
 *
 * @param context a Kerberos 5 context.
 * @param ctx a krb5_init_creds_context context.
 *
 * @return 0 for success, or an Kerberos 5 error code, see krb5_get_error_message().
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_warn_user(krb5_context context,
			  krb5_init_creds_context ctx)
{
    krb5_timestamp sec;
    krb5_const_realm realm;
    LastReq *lr;
    unsigned i;
    time_t t;

    if (ctx->prompter == NULL)
        return 0;

    if (ctx->warned_user)
	return 0;

    ctx->warned_user = 1;

    krb5_timeofday (context, &sec);

    realm = krb5_principal_get_realm (context, ctx->cred.client);
    lr = &ctx->enc_part.last_req;

    t = sec + get_config_time (context,
			       realm,
			       "warn_pwexpire",
			       7 * 24 * 60 * 60);

    for (i = 0; i < lr->len; ++i) {
	if (lr->val[i].lr_value <= t) {
	    switch (abs(lr->val[i].lr_type)) {
	    case LR_PW_EXPTIME :
		report_expiration(context, ctx->prompter,
				  ctx->prompter_data,
				  "Your password will expire at ",
				  lr->val[i].lr_value);
		break;
	    case LR_ACCT_EXPTIME :
		report_expiration(context, ctx->prompter,
				  ctx->prompter_data,
				  "Your account will expire at ",
				  lr->val[i].lr_value);
		break;
	    }
	}
    }

    return 0;
}

static krb5_addresses no_addrs = { 0, NULL };

static krb5_error_code
get_init_creds_common(krb5_context context,
		      krb5_principal client,
		      krb5_deltat start_time,
		      krb5_get_init_creds_opt *options,
		      krb5_init_creds_context ctx)
{
    krb5_get_init_creds_opt *default_opt = NULL;
    krb5_error_code ret;
    krb5_enctype *etypes;
    krb5_preauthtype *pre_auth_types;

    memset(ctx, 0, sizeof(*ctx));

    if (options == NULL) {
	const char *realm = krb5_principal_get_realm(context, client);

        krb5_get_init_creds_opt_alloc (context, &default_opt);
	options = default_opt;
	krb5_get_init_creds_opt_set_default_flags(context, NULL, realm, options);
    }

    if (options->opt_private) {
	if (options->opt_private->password) {
	    ret = krb5_init_creds_set_password(context, ctx,
					       options->opt_private->password);
	    if (ret)
		goto out;
	}

	ctx->keyproc = options->opt_private->key_proc;
	ctx->req_pac = options->opt_private->req_pac;
	ctx->pk_init_ctx = options->opt_private->pk_init_ctx;
	ctx->ic_flags = options->opt_private->flags;
    } else
	ctx->req_pac = KRB5_INIT_CREDS_TRISTATE_UNSET;

    if (ctx->keyproc == NULL)
	ctx->keyproc = default_s2k_func;

    /* Enterprise name implicitly turns on canonicalize */
    if ((ctx->ic_flags & KRB5_INIT_CREDS_CANONICALIZE) ||
	krb5_principal_get_type(context, client) == KRB5_NT_ENTERPRISE_PRINCIPAL)
	ctx->flags.canonicalize = 1;

    ctx->pre_auth_types = NULL;
    ctx->addrs = NULL;
    ctx->etypes = NULL;
    ctx->pre_auth_types = NULL;

    ret = init_cred(context, &ctx->cred, client, start_time, options);
    if (ret) {
	if (default_opt)
	    krb5_get_init_creds_opt_free(context, default_opt);
	return ret;
    }

    ret = krb5_init_creds_set_service(context, ctx, NULL);
    if (ret)
	goto out;

    if (options->flags & KRB5_GET_INIT_CREDS_OPT_FORWARDABLE)
	ctx->flags.forwardable = options->forwardable;

    if (options->flags & KRB5_GET_INIT_CREDS_OPT_PROXIABLE)
	ctx->flags.proxiable = options->proxiable;

    if (start_time)
	ctx->flags.postdated = 1;
    if (ctx->cred.times.renew_till)
	ctx->flags.renewable = 1;
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST) {
	ctx->addrs = options->address_list;
    } else if (options->opt_private) {
	switch (options->opt_private->addressless) {
	case KRB5_INIT_CREDS_TRISTATE_UNSET:
#if KRB5_ADDRESSLESS_DEFAULT == TRUE
	    ctx->addrs = &no_addrs;
#else
	    ctx->addrs = NULL;
#endif
	    break;
	case KRB5_INIT_CREDS_TRISTATE_FALSE:
	    ctx->addrs = NULL;
	    break;
	case KRB5_INIT_CREDS_TRISTATE_TRUE:
	    ctx->addrs = &no_addrs;
	    break;
	}
    }
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST) {
	if (ctx->etypes)
	    free(ctx->etypes);

	etypes = malloc((options->etype_list_length + 1)
			* sizeof(krb5_enctype));
	if (etypes == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto out;
	}
	memcpy (etypes, options->etype_list,
		options->etype_list_length * sizeof(krb5_enctype));
	etypes[options->etype_list_length] = ETYPE_NULL;
	ctx->etypes = etypes;
    }
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST) {
	pre_auth_types = malloc((options->preauth_list_length + 1)
				* sizeof(krb5_preauthtype));
	if (pre_auth_types == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto out;
	}
	memcpy (pre_auth_types, options->preauth_list,
		options->preauth_list_length * sizeof(krb5_preauthtype));
	pre_auth_types[options->preauth_list_length] = KRB5_PADATA_NONE;
	ctx->pre_auth_types = pre_auth_types;
    }
    if (options->flags & KRB5_GET_INIT_CREDS_OPT_ANONYMOUS)
	ctx->flags.request_anonymous = options->anonymous;
    if (default_opt)
        krb5_get_init_creds_opt_free(context, default_opt);

    return 0;
 out:
    if (default_opt)
	krb5_get_init_creds_opt_free(context, default_opt);
    return ret;
}

static krb5_error_code
change_password (krb5_context context,
		 krb5_principal client,
		 const char *password,
		 char *newpw,
		 size_t newpw_sz,
		 krb5_prompter_fct prompter,
		 void *data,
		 krb5_get_init_creds_opt *old_options)
{
    krb5_prompt prompts[2];
    krb5_error_code ret;
    krb5_creds cpw_cred;
    char buf1[BUFSIZ], buf2[BUFSIZ];
    krb5_data password_data[2];
    int result_code;
    krb5_data result_code_string;
    krb5_data result_string;
    char *p;
    krb5_get_init_creds_opt *options;

    memset (&cpw_cred, 0, sizeof(cpw_cred));

    ret = krb5_get_init_creds_opt_alloc(context, &options);
    if (ret)
        return ret;
    krb5_get_init_creds_opt_set_tkt_life (options, 60);
    krb5_get_init_creds_opt_set_forwardable (options, FALSE);
    krb5_get_init_creds_opt_set_proxiable (options, FALSE);
    if (old_options && old_options->flags & KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST)
	krb5_get_init_creds_opt_set_preauth_list (options,
						  old_options->preauth_list,
						  old_options->preauth_list_length);

    krb5_data_zero (&result_code_string);
    krb5_data_zero (&result_string);

    ret = krb5_get_init_creds_password (context,
					&cpw_cred,
					client,
					password,
					prompter,
					data,
					0,
					"kadmin/changepw",
					options);
    krb5_get_init_creds_opt_free(context, options);
    if (ret)
	goto out;

    for(;;) {
	password_data[0].data   = buf1;
	password_data[0].length = sizeof(buf1);

	prompts[0].hidden = 1;
	prompts[0].prompt = "New password: ";
	prompts[0].reply  = &password_data[0];
	prompts[0].type   = KRB5_PROMPT_TYPE_NEW_PASSWORD;

	password_data[1].data   = buf2;
	password_data[1].length = sizeof(buf2);

	prompts[1].hidden = 1;
	prompts[1].prompt = "Repeat new password: ";
	prompts[1].reply  = &password_data[1];
	prompts[1].type   = KRB5_PROMPT_TYPE_NEW_PASSWORD_AGAIN;

	ret = (*prompter) (context, data, NULL, "Changing password",
			   2, prompts);
	if (ret) {
	    ret = KRB5_LIBOS_PWDINTR;
	    memset (buf1, 0, sizeof(buf1));
	    memset (buf2, 0, sizeof(buf2));
	    goto out;
	}

	if (strcmp (buf1, buf2) == 0)
	    break;
	memset (buf1, 0, sizeof(buf1));
	memset (buf2, 0, sizeof(buf2));
    }

    ret = krb5_set_password (context,
			     &cpw_cred,
			     buf1,
			     NULL,
			     &result_code,
			     &result_code_string,
			     &result_string);
    if (ret)
	goto out;

    if (result_code == 0) {
	p = strdup("Success");
    } else if (asprintf(&p, "Failed: %.*s %.*s: %d\n",
			(int)result_code_string.length,
			result_code_string.length > 0 ? (char*)result_code_string.data : "",
			(int)result_string.length,
			result_string.length > 0 ? (char*)result_string.data : "",
			result_code) < 0)
    {
	ret = ENOMEM;
	goto out;
    }

    /* return the result */
    (*prompter) (context, data, NULL, p, 0, NULL);

    if (result_code == 0) {
	strlcpy (newpw, buf1, newpw_sz);
	ret = 0;
    } else {
	ret = ENOTTY;
	krb5_set_error_message(context, ret,
			       N_("failed changing password: %s", ""), p);
    }
    free (p);

out:
    memset (buf1, 0, sizeof(buf1));
    memset (buf2, 0, sizeof(buf2));
    krb5_data_free (&result_string);
    krb5_data_free (&result_code_string);
    krb5_free_cred_contents (context, &cpw_cred);
    return ret;
}


KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_keyblock_key_proc (krb5_context context,
			krb5_keytype type,
			krb5_data *salt,
			krb5_const_pointer keyseed,
			krb5_keyblock **key)
{
    return krb5_copy_keyblock (context, keyseed, key);
}

/*
 *
 */

static krb5_error_code
init_as_req (krb5_context context,
	     KDCOptions opts,
	     const krb5_creds *creds,
	     const krb5_addresses *addrs,
	     const krb5_enctype *etypes,
	     AS_REQ *a)
{
    krb5_error_code ret;

    memset(a, 0, sizeof(*a));

    a->pvno = 5;
    a->msg_type = krb_as_req;
    a->req_body.kdc_options = opts;
    a->req_body.cname = malloc(sizeof(*a->req_body.cname));
    if (a->req_body.cname == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	goto fail;
    }
    a->req_body.sname = calloc(1, sizeof(*a->req_body.sname));
    if (a->req_body.sname == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	goto fail;
    }

    ret = _krb5_principal2principalname (a->req_body.cname, creds->client);
    if (ret)
	goto fail;
    ret = copy_Realm(&creds->client->realm, &a->req_body.realm);
    if (ret)
	goto fail;

    ret = _krb5_principal2principalname (a->req_body.sname, creds->server);
    if (ret)
	goto fail;

    if(creds->times.starttime) {
	a->req_body.from = malloc(sizeof(*a->req_body.from));
	if (a->req_body.from == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto fail;
	}
	*a->req_body.from = creds->times.starttime;
    }
    if(creds->times.endtime){
	ALLOC(a->req_body.till, 1);
	*a->req_body.till = creds->times.endtime;
    }
    if(creds->times.renew_till){
	a->req_body.rtime = malloc(sizeof(*a->req_body.rtime));
	if (a->req_body.rtime == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto fail;
	}
	*a->req_body.rtime = creds->times.renew_till;
    }
    a->req_body.nonce = 0;
    ret = _krb5_init_etype(context,
			   KRB5_PDU_AS_REQUEST,
			   &a->req_body.etype.len,
			   &a->req_body.etype.val,
			   etypes);
    if (ret)
	goto fail;

    /*
     * This means no addresses
     */

    if (addrs && addrs->len == 0) {
	a->req_body.addresses = NULL;
    } else {
	a->req_body.addresses = malloc(sizeof(*a->req_body.addresses));
	if (a->req_body.addresses == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto fail;
	}

	if (addrs)
	    ret = krb5_copy_addresses(context, addrs, a->req_body.addresses);
	else {
	    ret = krb5_get_all_client_addrs (context, a->req_body.addresses);
	    if(ret == 0 && a->req_body.addresses->len == 0) {
		free(a->req_body.addresses);
		a->req_body.addresses = NULL;
	    }
	}
	if (ret)
	    goto fail;
    }

    a->req_body.enc_authorization_data = NULL;
    a->req_body.additional_tickets = NULL;

    a->padata = NULL;

    return 0;
 fail:
    free_AS_REQ(a);
    memset(a, 0, sizeof(*a));
    return ret;
}

static krb5_error_code
set_paid(struct pa_info_data *paid, krb5_context context,
	 krb5_enctype etype,
	 krb5_salttype salttype, void *salt_string, size_t salt_len,
	 krb5_data *s2kparams)
{
    paid->etype = etype;
    paid->salt.salttype = salttype;
    paid->salt.saltvalue.data = malloc(salt_len + 1);
    if (paid->salt.saltvalue.data == NULL) {
	krb5_clear_error_message(context);
	return ENOMEM;
    }
    memcpy(paid->salt.saltvalue.data, salt_string, salt_len);
    ((char *)paid->salt.saltvalue.data)[salt_len] = '\0';
    paid->salt.saltvalue.length = salt_len;
    if (s2kparams) {
	krb5_error_code ret;

	ret = krb5_copy_data(context, s2kparams, &paid->s2kparams);
	if (ret) {
	    krb5_clear_error_message(context);
	    krb5_free_salt(context, paid->salt);
	    return ret;
	}
    } else
	paid->s2kparams = NULL;

    return 0;
}

static struct pa_info_data *
pa_etype_info2(krb5_context context,
	       const krb5_principal client,
	       const AS_REQ *asreq,
	       struct pa_info_data *paid,
	       heim_octet_string *data)
{
    krb5_error_code ret;
    ETYPE_INFO2 e;
    size_t sz;
    size_t i, j;

    memset(&e, 0, sizeof(e));
    ret = decode_ETYPE_INFO2(data->data, data->length, &e, &sz);
    if (ret)
	goto out;
    if (e.len == 0)
	goto out;
    for (j = 0; j < asreq->req_body.etype.len; j++) {
	for (i = 0; i < e.len; i++) {
	    if (asreq->req_body.etype.val[j] == e.val[i].etype) {
		krb5_salt salt;
		if (e.val[i].salt == NULL)
		    ret = krb5_get_pw_salt(context, client, &salt);
		else {
		    salt.saltvalue.data = *e.val[i].salt;
		    salt.saltvalue.length = strlen(*e.val[i].salt);
		    ret = 0;
		}
		if (ret == 0)
		    ret = set_paid(paid, context, e.val[i].etype,
				   KRB5_PW_SALT,
				   salt.saltvalue.data,
				   salt.saltvalue.length,
				   e.val[i].s2kparams);
		if (e.val[i].salt == NULL)
		    krb5_free_salt(context, salt);
		if (ret == 0) {
		    free_ETYPE_INFO2(&e);
		    return paid;
		}
	    }
	}
    }
 out:
    free_ETYPE_INFO2(&e);
    return NULL;
}

static struct pa_info_data *
pa_etype_info(krb5_context context,
	      const krb5_principal client,
	      const AS_REQ *asreq,
	      struct pa_info_data *paid,
	      heim_octet_string *data)
{
    krb5_error_code ret;
    ETYPE_INFO e;
    size_t sz;
    size_t i, j;

    memset(&e, 0, sizeof(e));
    ret = decode_ETYPE_INFO(data->data, data->length, &e, &sz);
    if (ret)
	goto out;
    if (e.len == 0)
	goto out;
    for (j = 0; j < asreq->req_body.etype.len; j++) {
	for (i = 0; i < e.len; i++) {
	    if (asreq->req_body.etype.val[j] == e.val[i].etype) {
		krb5_salt salt;
		salt.salttype = KRB5_PW_SALT;
		if (e.val[i].salt == NULL)
		    ret = krb5_get_pw_salt(context, client, &salt);
		else {
		    salt.saltvalue = *e.val[i].salt;
		    ret = 0;
		}
		if (e.val[i].salttype)
		    salt.salttype = *e.val[i].salttype;
		if (ret == 0) {
		    ret = set_paid(paid, context, e.val[i].etype,
				   salt.salttype,
				   salt.saltvalue.data,
				   salt.saltvalue.length,
				   NULL);
		    if (e.val[i].salt == NULL)
			krb5_free_salt(context, salt);
		}
		if (ret == 0) {
		    free_ETYPE_INFO(&e);
		    return paid;
		}
	    }
	}
    }
 out:
    free_ETYPE_INFO(&e);
    return NULL;
}

static struct pa_info_data *
pa_pw_or_afs3_salt(krb5_context context,
		   const krb5_principal client,
		   const AS_REQ *asreq,
		   struct pa_info_data *paid,
		   heim_octet_string *data)
{
    krb5_error_code ret;
    if (paid->etype == KRB5_ENCTYPE_NULL)
	return NULL;
    ret = set_paid(paid, context,
		   paid->etype,
		   paid->salt.salttype,
		   data->data,
		   data->length,
		   NULL);
    if (ret)
	return NULL;
    return paid;
}


static krb5_error_code
make_pa_enc_timestamp(krb5_context context, METHOD_DATA *md,
		      krb5_enctype etype, krb5_keyblock *key)
{
    PA_ENC_TS_ENC p;
    unsigned char *buf;
    size_t buf_size;
    size_t len = 0;
    EncryptedData encdata;
    krb5_error_code ret;
    int32_t usec;
    int usec2;
    krb5_crypto crypto;

    krb5_us_timeofday (context, &p.patimestamp, &usec);
    usec2         = usec;
    p.pausec      = &usec2;

    ASN1_MALLOC_ENCODE(PA_ENC_TS_ENC, buf, buf_size, &p, &len, ret);
    if (ret)
	return ret;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret) {
	free(buf);
	return ret;
    }
    ret = krb5_encrypt_EncryptedData(context,
				     crypto,
				     KRB5_KU_PA_ENC_TIMESTAMP,
				     buf,
				     len,
				     0,
				     &encdata);
    free(buf);
    krb5_crypto_destroy(context, crypto);
    if (ret)
	return ret;

    ASN1_MALLOC_ENCODE(EncryptedData, buf, buf_size, &encdata, &len, ret);
    free_EncryptedData(&encdata);
    if (ret)
	return ret;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_padata_add(context, md, KRB5_PADATA_ENC_TIMESTAMP, buf, len);
    if (ret)
	free(buf);
    return ret;
}

static krb5_error_code
add_enc_ts_padata(krb5_context context,
		  METHOD_DATA *md,
		  krb5_principal client,
		  krb5_s2k_proc keyproc,
		  krb5_const_pointer keyseed,
		  krb5_enctype *enctypes,
		  unsigned netypes,
		  krb5_salt *salt,
		  krb5_data *s2kparams)
{
    krb5_error_code ret;
    krb5_salt salt2;
    krb5_enctype *ep;
    size_t i;

    memset(&salt2, 0, sizeof(salt2));

    if(salt == NULL) {
	/* default to standard salt */
	ret = krb5_get_pw_salt (context, client, &salt2);
	if (ret)
	    return ret;
	salt = &salt2;
    }
    if (!enctypes) {
	enctypes = context->etypes;
	netypes = 0;
	for (ep = enctypes; *ep != ETYPE_NULL; ep++)
	    netypes++;
    }

    for (i = 0; i < netypes; ++i) {
	krb5_keyblock *key;

	_krb5_debugx(context, 5, "krb5_get_init_creds: using ENC-TS with enctype %d", enctypes[i]);

	ret = (*keyproc)(context, enctypes[i], keyseed,
			 *salt, s2kparams, &key);
	if (ret)
	    continue;
	ret = make_pa_enc_timestamp (context, md, enctypes[i], key);
	krb5_free_keyblock (context, key);
	if (ret)
	    return ret;
    }
    if(salt == &salt2)
	krb5_free_salt(context, salt2);
    return 0;
}

static krb5_error_code
pa_data_to_md_ts_enc(krb5_context context,
		     const AS_REQ *a,
		     const krb5_principal client,
		     krb5_init_creds_context ctx,
		     struct pa_info_data *ppaid,
		     METHOD_DATA *md)
{
    if (ctx->keyproc == NULL || ctx->keyseed == NULL) {
	_krb5_debugx(context, 5, "krb5_get_init_creds: no keyproc or keyseed");
	return 0;
    }

    if (ppaid) {
	_krb5_debugx(context, 5, "krb5_get_init_creds: pa-info found, using %d", (int)ppaid->etype);

	add_enc_ts_padata(context, md, client,
			  ctx->keyproc, ctx->keyseed,
			  &ppaid->etype, 1,
			  &ppaid->salt, ppaid->s2kparams);
    } else {
	krb5_salt salt;

	_krb5_debugx(context, 5, "krb5_get_init_creds: pa-info not found, guessing salt");

	/* make a v5 salted pa-data */
	add_enc_ts_padata(context, md, client,
			  ctx->keyproc, ctx->keyseed,
			  a->req_body.etype.val, a->req_body.etype.len,
			  NULL, NULL);

	/* make a v4 salted pa-data */
	salt.salttype = KRB5_PW_SALT;
	krb5_data_zero(&salt.saltvalue);
	add_enc_ts_padata(context, md, client,
			  ctx->keyproc, ctx->keyseed,
			  a->req_body.etype.val, a->req_body.etype.len,
			  &salt, NULL);
    }
    return 0;
}

static krb5_error_code
pa_data_to_key_plain(krb5_context context,
		     const krb5_principal client,
		     krb5_init_creds_context ctx,
		     krb5_salt salt,
		     krb5_data *s2kparams,
		     krb5_enctype etype,
		     krb5_keyblock **key)
{
    krb5_error_code ret;

    ret = (*ctx->keyproc)(context, etype, ctx->keyseed,
			   salt, s2kparams, key);
    return ret;
}


static krb5_error_code
pa_data_to_md_pkinit(krb5_context context,
		     const AS_REQ *a,
		     const krb5_principal client,
		     int win2k,
		     krb5_init_creds_context ctx,
		     METHOD_DATA *md)
{
    if (ctx->pk_init_ctx == NULL)
	return 0;
#ifdef PKINIT
    return _krb5_pk_mk_padata(context,
			      ctx->pk_init_ctx,
			      ctx->ic_flags,
			      win2k,
			      &a->req_body,
			      ctx->pk_nonce,
			      md);
#else
    krb5_set_error_message(context, EINVAL,
			   N_("no support for PKINIT compiled in", ""));
    return EINVAL;
#endif
}

static krb5_error_code
pkinit_configure(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx)
{
    if (ctx->pk_init_ctx == NULL)
	return HEIM_ERR_PA_CANT_CONTINUE;

    return 0;
}

static krb5_error_code
pkinit_step(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx, PA_DATA *pa, const AS_REQ *a,
	    const AS_REP *rep, const krb5_krbhst_info *hi, METHOD_DATA *in_md, METHOD_DATA *out_md)
{
    if (rep == NULL) {
	krb5_error_code ret;
	ret = pa_data_to_md_pkinit(context, a, ctx->cred.client, 0, ctx, out_md);
	if (ret == 0)
	    ret = HEIM_ERR_PA_CONTINUE_NEEDED;
	return ret;
    } else {
	return _krb5_pk_rd_pa_reply(context,
				    a->req_body.realm,
				    ctx->pk_init_ctx,
				    rep->enc_part.etype,
				    hi,
				    ctx->pk_nonce,
				    &ctx->req_buffer,
				    pa,
				    &ctx->fast_state.reply_key);
    }
}

static krb5_error_code
pkinit_step_win(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx, PA_DATA *pa, const AS_REQ *a,
		const AS_REP *rep, const krb5_krbhst_info *hi, METHOD_DATA *in_md, METHOD_DATA *out_md)
{
    if (rep == NULL) {
	krb5_error_code ret;
	ret = pa_data_to_md_pkinit(context, a, ctx->cred.client, 1, ctx, out_md);
	if (ret == 0)
	    ret = HEIM_ERR_PA_CONTINUE_NEEDED;
	return ret;
    } else {
	return _krb5_pk_rd_pa_reply(context,
				    a->req_body.realm,
				    ctx->pk_init_ctx,
				    rep->enc_part.etype,
				    hi,
				    ctx->pk_nonce,
				    &ctx->req_buffer,
				    pa,
				    &ctx->fast_state.reply_key);
    }
}

static void
pkinit_release(void *pa_ctx)
{
}

krb5_error_code
_krb5_make_pa_enc_challange(krb5_context context,
			    krb5_crypto crypto,
			    krb5_key_usage usage,
			    METHOD_DATA *md)
{
    PA_ENC_TS_ENC p;
    unsigned char *buf;
    size_t buf_size;
    size_t len = 0;
    EncryptedData encdata;
    krb5_error_code ret;
    int32_t usec;
    int usec2;

    krb5_us_timeofday (context, &p.patimestamp, &usec);
    usec2         = usec;
    p.pausec      = &usec2;

    ASN1_MALLOC_ENCODE(PA_ENC_TS_ENC, buf, buf_size, &p, &len, ret);
    if (ret)
	return ret;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_encrypt_EncryptedData(context,
				     crypto,
				     usage,
				     buf,
				     len,
				     0,
				     &encdata);
    free(buf);
    if (ret)
	return ret;

    ASN1_MALLOC_ENCODE(EncryptedData, buf, buf_size, &encdata, &len, ret);
    free_EncryptedData(&encdata);
    if (ret)
	return ret;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_padata_add(context, md, KRB5_PADATA_ENCRYPTED_CHALLENGE, buf, len);
    if (ret)
	free(buf);
    return ret;
}

krb5_error_code
_krb5_validate_pa_enc_challange(krb5_context context,
				krb5_crypto crypto,
				krb5_key_usage usage,
				EncryptedData *enc_data,
				const char *peer_name)
{
    krb5_error_code ret;
    krb5_data ts_data;
    PA_ENC_TS_ENC p;
    time_t timestamp;
    int32_t usec;
    size_t size;

    ret = krb5_decrypt_EncryptedData(context, crypto, usage, enc_data, &ts_data);
    if (ret)
	return ret;
	
    ret = decode_PA_ENC_TS_ENC(ts_data.data,
			       ts_data.length,
			       &p,
			       &size);
    krb5_data_free(&ts_data);
    if(ret){
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	_krb5_debugx(context, 5, "Failed to decode PA-ENC-TS_ENC - %s", peer_name);
	goto out;
    }

    krb5_us_timeofday(context, &timestamp, &usec);

    if (krb5_time_abs(timestamp, p.patimestamp) > context->max_skew) {
	char client_time[100];

	krb5_format_time(context, p.patimestamp,
			 client_time, sizeof(client_time), TRUE);
	
	ret = KRB5KRB_AP_ERR_SKEW;
	_krb5_debugx(context, 0, "Too large time skew, "
		     "client time %s is out by %u > %d seconds -- %s",
		     client_time,
		     (unsigned)krb5_time_abs(timestamp, p.patimestamp),
		     (int)context->max_skew,
		     peer_name);
    } else {
	ret = 0;
    }
    
 out:
    free_PA_ENC_TS_ENC(&p);

    return ret;
}


static struct pa_info_data *
process_pa_info(krb5_context, const krb5_principal, const AS_REQ *, struct pa_info_data *, METHOD_DATA *);


static krb5_error_code
enc_chal_step(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx, PA_DATA *pa, const AS_REQ *a,
	      const AS_REP *rep, const krb5_krbhst_info *hi, METHOD_DATA *in_md, METHOD_DATA *out_md)
{
    struct pa_info_data paid, *ppaid;
    krb5_keyblock challengekey;
    krb5_data pepper1, pepper2;
    krb5_crypto crypto = NULL;
    krb5_enctype aenctype;
    krb5_error_code ret;

    memset(&paid, 0, sizeof(paid));

    if (rep == NULL)
	paid.etype = KRB5_ENCTYPE_NULL;
    else
	paid.etype = rep->enc_part.etype;
    ppaid = process_pa_info(context, ctx->cred.client, a, &paid, in_md);

    /*
     * If we don't have ppaid, ts because the KDC have not sent any
     * salt info, lets to the first roundtrip so the KDC have a chance
     * to send any.
     */
    if (ppaid == NULL) {
	_krb5_debugx(context, 5, "no ppaid found");
	return HEIM_ERR_PA_CONTINUE_NEEDED;
    }
    if (ppaid->etype == ETYPE_NULL) {
	return HEIM_ERR_PA_CANT_CONTINUE;
    }

    if (ctx->fast_state.reply_key)
	krb5_free_keyblock(context, ctx->fast_state.reply_key);

    ret = pa_data_to_key_plain(context, ctx->cred.client, ctx,
			       ppaid->salt, ppaid->s2kparams, ppaid->etype,
			       &ctx->fast_state.reply_key);
    free_paid(context, &paid);
    if (ret) {
	_krb5_debugx(context, 5, "enc-chal: failed to build key");
	return ret;
    }

    ret = krb5_crypto_init(context, ctx->fast_state.reply_key, 0, &crypto);
    if (ret)
	return ret;
	
    krb5_crypto_getenctype(context, ctx->fast_state.armor_crypto, &aenctype);
    
    pepper1.data = rep ? "kdcchallengearmor" : "clientchallengearmor";
    pepper1.length = strlen(pepper1.data);
    pepper2.data = "challengelongterm";
    pepper2.length = strlen(pepper2.data);

    ret = krb5_crypto_fx_cf2(context, ctx->fast_state.armor_crypto, crypto,
			     &pepper1, &pepper2, aenctype,
			     &challengekey);
    krb5_crypto_destroy(context, crypto);

    ret = krb5_crypto_init(context, &challengekey, 0, &crypto);
    krb5_free_keyblock_contents(context, &challengekey);
    if (ret)
	return ret;

    if (rep) {
	EncryptedData enc_data;
	size_t size;

	if (ret) {
	    _krb5_debugx(context, 5, "enc-chal: failed to create reply key");
	    return ret;
	}

	_krb5_debugx(context, 5, "ENC_CHAL rep key");

	if (ctx->fast_state.strengthen_key == NULL) {
	    krb5_crypto_destroy(context, crypto);
	    _krb5_debugx(context, 5, "ENC_CHAL w/o strengthen_key");
	    return KRB5_KDCREP_MODIFIED;
	}

	if (pa == NULL) {
	    krb5_crypto_destroy(context, crypto);
	    _krb5_debugx(context, 0, "KDC response missing");
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = decode_EncryptedData(pa->padata_value.data,
				   pa->padata_value.length,
				   &enc_data,
				   &size);
	if (ret) {
	    ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
	    _krb5_debugx(context, 5, "Failed to decode ENC_CHAL KDC reply");
	    return ret;
	}

	ret = _krb5_validate_pa_enc_challange(context, crypto,
					      KRB5_KU_ENC_CHALLENGE_KDC,
					      &enc_data,
					      "KDC");
	free_EncryptedData(&enc_data);
	krb5_crypto_destroy(context, crypto);

	return ret;

    } else {

	ret = _krb5_make_pa_enc_challange(context, crypto, 
					  KRB5_KU_ENC_CHALLENGE_CLIENT,
					  out_md);
	krb5_crypto_destroy(context, crypto);
	if (ret) {
	    _krb5_debugx(context, 5, "enc-chal: failed build enc challange");
	    return ret;
	}

	return HEIM_ERR_PA_CONTINUE_NEEDED;
    }
}

#ifdef __APPLE_PRIVATE__

/*
 * SRP
 */

#include <corecrypto/ccsrp.h>
#include <corecrypto/ccsrp_gp.h>
#include <corecrypto/ccdh.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccpbkdf2.h>
#include <CommonCrypto/CommonRandomSPI.h>

/*
 * ordered in preference
 */
static const struct _krb5_srp_group {
    enum KRB5_SRP_GROUP group;
    ccdh_const_gp_t (*gp)(void);
    const struct ccdigest_info *(*di)(void);
    const heim_oid *oid;
} srpgroups[] = {
    {
	KRB5_SRP_GROUP_RFC5054_4096_PBKDF2_SHA512,
	ccsrp_gp_rfc5054_4096,
	ccsha512_di,
	&asn1_oid_id_pkinit_kdf_ah_sha512
    }
};

/*
 *
 */

const struct _krb5_srp_group *
_krb5_srp_validate_group(KRB5_SRP_GROUP group)
{
    size_t n;

    for (n = 0; n < sizeof(srpgroups)/sizeof(srpgroups[0]); n++)
	if (srpgroups[n].group == group)
	    return &srpgroups[n];

    return NULL;
}


size_t
_krb5_srp_pkisize(const struct _krb5_srp_group *group)
{
    return ccdh_gp_prime_size(group->gp());
}

size_t
_krb5_srp_keysize(const struct _krb5_srp_group *group)
{
    return group->di()->output_size;
}

struct _krb5_srp *
_krb5_srp_create(const struct _krb5_srp_group *group)
{
    const struct ccdigest_info *di = group->di();
    ccsrp_const_gp_t gp = group->gp();
    ccsrp_ctx * srp;
    
    srp = malloc(ccsrp_sizeof_srp(di, gp));
    if (srp == NULL)
	return NULL;

    ccsrp_ctx_init(srp, di, gp);
    
    return (struct _krb5_srp *)srp;
}

krb5_error_code
_krb5_srp_create_pa(krb5_context context, 
		    const struct _krb5_srp_group *group,
		    krb5_const_principal principal,
		    const char *password,
		    const KRB5_SRP_PA *spa,
		    krb5_data *verifier)
{
    krb5_error_code ret;
    char *username;
    ccsrp_ctx *srpctx;
    krb5_data key;

    ret = krb5_data_alloc(verifier, _krb5_srp_pkisize(group));
    if (ret)
	return ret;

    ret = krb5_unparse_name_flags(context, principal, KRB5_PRINCIPAL_UNPARSE_NO_REALM, &username);
    if (ret) {
	krb5_data_free(verifier);
	return ret;
    }

    ret = krb5_data_alloc(&key, _krb5_srp_keysize(group));
    if (ret) {
	free(username);
	krb5_data_free(verifier);
	return ret;
    }

    ret = ccpbkdf2_hmac(group->di(),
			strlen(password), password,
			spa->salt.length, spa->salt.data,
			spa->iterations, _krb5_srp_keysize(group),
			key.data);
    
    srpctx = (ccsrp_ctx *)_krb5_srp_create(group);
    if (srpctx == NULL) {
	krb5_data_free(verifier);
	krb5_data_free(&key);
	krb5_xfree(username);
	return ENOMEM;
    }

    ret = ccsrp_generate_verifier(srpctx, username,
				  key.length, key.data,
				  spa->salt.length, spa->salt.data,
				  verifier->data);
    krb5_data_free(&key);
    krb5_xfree(srpctx);
    krb5_xfree(username);
    if (ret)
	return EINVAL;

    return 0;
}

krb5_error_code
_krb5_srp_reply_key(krb5_context context,
		    const struct _krb5_srp_group *srpgroup,
		    krb5_enctype enctype,
		    const void *session_key,
		    size_t session_key_length,
		    krb5_const_principal client,
		    krb5_data *req_buffer,
		    krb5_data *announce,
		    krb5_keyblock *reply_key)
{
    AlgorithmIdentifier ai;
		    
    ai.algorithm = *srpgroup->oid;
    ai.parameters = NULL;

    return _krb5_pk_kdf(context,
			&ai,
			session_key,
			session_key_length,
			client,
			NULL,
			enctype,
			req_buffer,
			announce,
			NULL,
			reply_key);
}



/*
 * Client only bits below
 */

enum KRB5_SRP_STATE {
    KRB5_SRP_STATE_FIRST = 0,
    KRB5_SRP_STATE_INIT,
    KRB5_SRP_STATE_SERVER_CHALLENGE,
    KRB5_SRP_STATE_SERVER_VERIFIER,
    KRB5_SRP_STATE_DONE
};

typedef struct srp_state_data {
    enum KRB5_SRP_STATE state;
    const struct _krb5_srp_group *group;
    ccsrp_ctx *srp;
    size_t keylength;
    size_t pkilength;
    krb5_data key;
    krb5_data announce;
    KRB5_SRP_PA spa;
} *srp_state_t;

static krb5_error_code
srp_configure(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx)
{
    srp_state_t state = pa_ctx;

    if (ctx->password == NULL)
	return HEIM_ERR_PA_CANT_CONTINUE;

    state->state = KRB5_SRP_STATE_INIT;
    state->group = NULL;

    return 0;
}

static krb5_error_code
srp_step(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx, PA_DATA *pa, const AS_REQ *a,
	 const AS_REP *rep, const krb5_krbhst_info *hi, METHOD_DATA *in_md, METHOD_DATA *out_md)
{
    srp_state_t state = pa_ctx;
    krb5_error_code ret;

    if (pa == NULL) {
	_krb5_debugx(context, 0, "KDC didn't return any SRP pa data");
	state->state = KRB5_SRP_STATE_DONE;
	return HEIM_ERR_PA_CANT_CONTINUE;
    }

    if (state->state == KRB5_SRP_STATE_INIT) {
	KRB5_SRP_PA_ANNOUNCE sa;
	KRB5_SRP_PA *spa = NULL;
	KRB5_SRP_PA_INIT ci;
	krb5_data data;
	size_t size, n;

	memset(&ci, 0, sizeof(ci));

	ret = decode_KRB5_SRP_PA_ANNOUNCE(pa->padata_value.data, pa->padata_value.length, &sa, &size);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = krb5_data_copy(&state->announce, pa->padata_value.data, pa->padata_value.length);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_ANNOUNCE(&sa);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	/* pick first group KDC sends, the list is validated by mixing it into the kdf */
	for (n = 0; n < sa.groups.len; n++) {
	    state->group = _krb5_srp_validate_group(sa.groups.val[n].group);
	    if (state->group != NULL) {
		spa = &sa.groups.val[n];
		break;
	    }
	}
	if (state->group == NULL) {
	    _krb5_debugx(context, 0, "KDC didn't send a good SRP group for us, sent %u group(s)",
			 (unsigned)sa.groups.len);
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_ANNOUNCE(&sa);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = copy_KRB5_SRP_PA(spa, &state->spa);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_ANNOUNCE(&sa);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	state->srp = (ccsrp_ctx *)_krb5_srp_create(state->group);
	if (state->srp == NULL) {
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_ANNOUNCE(&sa);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}
	state->keylength = _krb5_srp_keysize(state->group);
	state->pkilength = _krb5_srp_pkisize(state->group);

	ret = krb5_data_alloc(&state->key, state->keylength);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_ANNOUNCE(&sa);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = ccpbkdf2_hmac(state->group->di(),
			    strlen(ctx->password), ctx->password,
			    spa->salt.length, spa->salt.data,
			    spa->iterations, state->keylength,
			    state->key.data);
	free_KRB5_SRP_PA_ANNOUNCE(&sa);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = krb5_data_alloc(&ci.a, state->pkilength);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	/*
	 *
	 */
	ci.group = state->group->group;

	ccsrp_client_start_authentication(state->srp, ccDevRandomGetRngState(), ci.a.data);

	ASN1_MALLOC_ENCODE(KRB5_SRP_PA_INIT, data.data, data.length, &ci, &size, ret);
	free_KRB5_SRP_PA_INIT(&ci);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}
	heim_assert(data.length == size, "ASN1.1 Internal error");

	ret = krb5_padata_add(context, out_md, KRB5_PADATA_SRP, data.data, data.length);
	if (ret) {
	    free(data.data);
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	state->state = KRB5_SRP_STATE_SERVER_CHALLENGE;

	return HEIM_ERR_PA_CONTINUE_NEEDED;

    } else if (state->state == KRB5_SRP_STATE_SERVER_CHALLENGE) {
	KRB5_SRP_PA_SERVER_CHALLENGE sc;
	KRB5_SRP_PA_CLIENT_RESPONSE cr;
	krb5_principal principal = NULL;
	krb5_data data;
	char *username;
	size_t size;
	 
	memset(&cr, 0, sizeof(cr));

	ret = decode_KRB5_SRP_PA_SERVER_CHALLENGE(pa->padata_value.data, pa->padata_value.length, &sc, &size);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}
	
	if (sc.length != state->pkilength) {
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_SERVER_CHALLENGE(&sc);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}
	heim_assert(a->req_body.cname != NULL, "should not get here since we could not have found the hdb entry otherwise");
	
	ret = _krb5_principalname2krb5_principal(context, &principal,
						 *a->req_body.cname,
						 a->req_body.realm);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_SERVER_CHALLENGE(&sc);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}


	ret = krb5_unparse_name_flags(context, principal, KRB5_PRINCIPAL_UNPARSE_NO_REALM, &username);
	krb5_free_principal(context, principal);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_SERVER_CHALLENGE(&sc);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = krb5_data_alloc(&cr, state->keylength);
	if (ret) {
	    free(username);
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_SERVER_CHALLENGE(&sc);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}
	
	_krb5_debugx(context, 5, "ccsrp client start for user: %s", username);

	ret = ccsrp_client_process_challenge(state->srp, username,
					     state->key.length,
					     state->key.data,
					     state->spa.salt.length,
					     state->spa.salt.data,
					     sc.data, cr.data);
	free_KRB5_SRP_PA_SERVER_CHALLENGE(&sc);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_CLIENT_RESPONSE(&cr);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ASN1_MALLOC_ENCODE(KRB5_SRP_PA_CLIENT_RESPONSE, data.data, data.length, &cr, &size, ret);
	free_KRB5_SRP_PA_CLIENT_RESPONSE(&cr);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}
	heim_assert(data.length == size, "ASN.1 internal error");

	ret = krb5_padata_add(context, out_md, KRB5_PADATA_SRP, data.data, data.length);
	if (ret) {
	    free(data.data);
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	state->state = KRB5_SRP_STATE_SERVER_VERIFIER;
	return HEIM_ERR_PA_CONTINUE_NEEDED;

    } else if (state->state == KRB5_SRP_STATE_SERVER_VERIFIER) {
	KRB5_SRP_PA_SERVER_VERIFIER sv;
	krb5_principal client = NULL;
	size_t size;
	bool boolres;

	if (rep == NULL) {
	    _krb5_debugx(context, 0, "KDC didn't return an AS-REP in last step of verifier");
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = decode_KRB5_SRP_PA_SERVER_VERIFIER(pa->padata_value.data, pa->padata_value.length, &sv, &size);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	if (sv.length != state->keylength) {
	    state->state = KRB5_SRP_STATE_DONE;
	    free_KRB5_SRP_PA_SERVER_VERIFIER(&sv);
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}
	
	boolres = ccsrp_client_verify_session(state->srp, sv.data);
	free_KRB5_SRP_PA_SERVER_VERIFIER(&sv);
	if (!boolres) {
	    _krb5_debugx(context, 0, "Failed to validate the KDC");
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	if (ctx->fast_state.reply_key)
	    krb5_free_keyblock(context, ctx->fast_state.reply_key);

	ctx->fast_state.reply_key = calloc(1, sizeof(*ctx->fast_state.reply_key));
	if (ctx->fast_state.reply_key == NULL) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = _krb5_principalname2krb5_principal(context, &client,
						 rep->cname, rep->crealm);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = _krb5_srp_reply_key(context,
				  state->group,
				  rep->enc_part.etype,
				  ccsrp_ctx_K(state->srp),
				  state->keylength,
				  client,
				  &ctx->req_buffer,
				  &state->announce,
				  ctx->fast_state.reply_key);
	krb5_free_principal(context, client);
	if (ret) {
	    state->state = KRB5_SRP_STATE_DONE;
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	state->state = KRB5_SRP_STATE_DONE;

	return 0;

    } else if (state->state == KRB5_SRP_STATE_DONE) {
	/* Called too many files */
	return HEIM_ERR_PA_CANT_CONTINUE;
    }

    krb5_abortx(context, "internal state machine error");
}

static void
srp_release(void *pa_ctx)
{
    srp_state_t state = pa_ctx;
    free(state->srp);
    krb5_data_free(&state->announce);
    krb5_data_free(&state->key);
    free_KRB5_SRP_PA(&state->spa);
}

#endif

struct enc_ts_context {
    int used_pa_types;
#define  USED_ENC_TS_GUESS	4
#define  USED_ENC_TS_INFO	8
#define  USED_ENC_TS_RENEG	16
    krb5_principal user;
};

static krb5_error_code
enc_ts_restart(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx)
{
    struct enc_ts_context *pactx = (struct enc_ts_context *)pa_ctx;
    pactx->used_pa_types = 0;
    krb5_free_principal(context, pactx->user);
    pactx->user = NULL;
    return 0;
}

static krb5_error_code
enc_ts_step(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx, PA_DATA *pa,
	    const AS_REQ *a, 
	    const AS_REP *rep,
	    const krb5_krbhst_info *hi,
	    METHOD_DATA *in_md, METHOD_DATA *out_md)
{
    struct enc_ts_context *pactx = (struct enc_ts_context *)pa_ctx;
    struct pa_info_data paid, *ppaid;
    krb5_error_code ret;
    const char *state;
    unsigned flag;

    /*
     * Keep track of the user we used so that we can restart
     * authentication when we get referals.
     */

    if (pactx->user && !krb5_principal_compare(context, pactx->user, ctx->cred.client)) {
	pactx->used_pa_types = 0;
	krb5_free_principal(context, pactx->user);
	pactx->user = NULL;
    }

    if (pactx->user == NULL) {
	ret = krb5_copy_principal(context, ctx->cred.client, &pactx->user);
	if (ret)
	    return ret;
    }

    memset(&paid, 0, sizeof(paid));

    if (rep == NULL)
	paid.etype = KRB5_ENCTYPE_NULL;
    else
	paid.etype = rep->enc_part.etype;

    ppaid = process_pa_info(context, ctx->cred.client, a, &paid, in_md);

    if (rep) {
	/*
	 * Some KDC's don't send salt info in the reply when there is
	 * success pre-auth happned before, so use cached copy (or
	 * even better, if there is just one pre-auth, save reply-key).
	 */
	if (ppaid == NULL && ctx->paid.etype != KRB5_ENCTYPE_NULL) {
	    ppaid = &ctx->paid;

	} else if (ppaid == NULL) {
	    _krb5_debugx(context, 0, "no paid when building key, build a default salt structure ?");
	    return HEIM_ERR_PA_CANT_CONTINUE;
	}

	ret = pa_data_to_key_plain(context, ctx->cred.client, ctx,
				   ppaid->salt, ppaid->s2kparams, rep->enc_part.etype,
				   &ctx->fast_state.reply_key);
	free_paid(context, &paid);
	return ret;
    }

    /*
     * If we don't have ppaid, ts because the KDC have not sent any
     * salt info, lets to the first roundtrip so the KDC have a chance
     * to send any.
     *
     * Don't bother guessing, it sounds like a good idea until you run
     * into KDCs that are doing failed auth counting based on the
     * ENC_TS tries.
     *
     * Stashing the salt for the next run is a diffrent issue and
     * could be considered in the future.
     */

    if (ppaid == NULL) {
	_krb5_debugx(context, 5,
		     "TS-ENC: waiting for KDC to set pw-salt/etype_info{,2}");
	return HEIM_ERR_PA_CONTINUE_NEEDED;
    }
    if (ppaid->etype == ETYPE_NULL) {
	free_paid(context, &paid);
	_krb5_debugx(context, 5,
		     "TS-ENC: kdc proposes enctype NULL ?");
	return HEIM_ERR_PA_CANT_CONTINUE;
    }

    /*
     * We have to allow the KDC to re-negotiate the PA-TS data
     * once, this is since the in the case of a windows read only
     * KDC that doesn't have the keys simply guesses what the
     * master is supposed to support. In the case where this
     * breaks in when the RO-KDC is a newer version the the RW-KDC
     * and the RO-KDC announced a enctype that the older doesn't
     * support.
     */
    if (pactx->used_pa_types & USED_ENC_TS_INFO) {
	flag = USED_ENC_TS_RENEG;
	state = "reneg";
    } else {
	flag = USED_ENC_TS_INFO;
	state = "info";
    }
    
    if (pactx->used_pa_types & flag) {
	free_paid(context, &paid);
	krb5_set_error_message(context, KRB5_GET_IN_TKT_LOOP,
			       "Already tried ENC-TS-%s, looping", state);
	return KRB5_GET_IN_TKT_LOOP;
    }
    
    pactx->used_pa_types |= flag;

    free_paid(context, &ctx->paid);
    ctx->paid = *ppaid;
    

    ret = pa_data_to_md_ts_enc(context, a, ctx->cred.client, ctx, ppaid, out_md);
    if (ret)
	return ret;

    return HEIM_ERR_PA_CONTINUE_NEEDED;
}

static void
enc_ts_release(void *pa_ctx)
{
    struct enc_ts_context *pactx = (struct enc_ts_context *)pa_ctx;

    if (pactx->user)
	krb5_free_principal(NULL, pactx->user);
	
}

static krb5_error_code
pa_pac_step(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx, PA_DATA *pa, const AS_REQ *a,
	    const AS_REP *rep, const krb5_krbhst_info *hi,
	    METHOD_DATA *in_md, METHOD_DATA *out_md)
{
    size_t len = 0, length;
    krb5_error_code ret;
    PA_PAC_REQUEST req;
    void *buf;

    switch (ctx->req_pac) {
    case KRB5_INIT_CREDS_TRISTATE_UNSET:
	return 0; /* don't bother */
    case KRB5_INIT_CREDS_TRISTATE_TRUE:
	req.include_pac = 1;
	break;
    case KRB5_INIT_CREDS_TRISTATE_FALSE:
	req.include_pac = 0;
    }

    ASN1_MALLOC_ENCODE(PA_PAC_REQUEST, buf, length,
		       &req, &len, ret);
    if (ret)
	return ret;
    heim_assert(len == length, "internal error in ASN.1 encoder");

    ret = krb5_padata_add(context, out_md, KRB5_PADATA_PA_PAC_REQUEST, buf, len);
    if (ret)
	free(buf);

    return 0;
}

static krb5_error_code
pa_pku2u_name_step(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx, PA_DATA *pa, const AS_REQ *a,
		   const AS_REP *rep, const krb5_krbhst_info *hi,
		   METHOD_DATA *in_md, METHOD_DATA *out_md)
{
#ifdef PKINIT
    if (ctx->pku2u_assertion) {
	krb5_error_code ret;
	krb5_data data;

	ret = krb5_data_copy(&data,
			     ctx->pku2u_assertion->data,
			     ctx->pku2u_assertion->length);
	if (ret)
	    return ret;
        ret = krb5_padata_add(context, out_md, KRB5_PADATA_PKU2U_NAME, 
			      data.data, data.length);
	if (ret) {
	    krb5_data_free(&data);
	    return ret;
	}
    }
#endif
    return 0;
}

static krb5_error_code
pa_enc_pa_rep_step(krb5_context context, krb5_init_creds_context ctx, void *pa_ctx, PA_DATA *pa, const AS_REQ *a,
		   const AS_REP *rep, const krb5_krbhst_info *hi,
		   METHOD_DATA *in_md, METHOD_DATA *out_md)
{
#ifdef PKINIT
    if (ctx->runflags.allow_enc_pa_rep)
        return krb5_padata_add(context, out_md, KRB5_PADATA_REQ_ENC_PA_REP, NULL, 0);
#endif
    return 0;
}

static krb5_error_code
pa_fx_cookie_step(krb5_context context,
		  krb5_init_creds_context ctx,
		  void *pa_ctx,
		  PA_DATA *pa,
		  const AS_REQ *a,
		  const AS_REP *rep,
		  const krb5_krbhst_info *hi,
		  METHOD_DATA *in_md,
		  METHOD_DATA *out_md)
{
    krb5_error_code ret;
    void *cookie;
    PA_DATA *pad;
    int idx = 0;

    pad = krb5_find_padata(in_md->val, in_md->len, KRB5_PADATA_FX_COOKIE, &idx);
    if (pad == NULL)
	return 0;

    cookie = malloc(pad->padata_value.length);
    if (cookie == NULL)
	return ENOMEM;

    memcpy(cookie, pad->padata_value.data, pad->padata_value.length);

    ret = krb5_padata_add(context, out_md, KRB5_PADATA_FX_COOKIE, cookie, pad->padata_value.length);
    if (ret)
	free(cookie);
    else {
	_krb5_debugx(context, 5, "Mirrored FX-COOKIE to KDC");
    }	

    return ret;
}




typedef struct pa_info_data *(*pa_salt_info_f)(krb5_context, const krb5_principal, const AS_REQ *, struct pa_info_data *, heim_octet_string *);
typedef krb5_error_code (*pa_configure_f)(krb5_context, krb5_init_creds_context, void *);
typedef krb5_error_code (*pa_restart_f)(krb5_context, krb5_init_creds_context, void *);
typedef krb5_error_code (*pa_step_f)(krb5_context, krb5_init_creds_context, void *, PA_DATA *, const AS_REQ *, const AS_REP *, const krb5_krbhst_info *, METHOD_DATA *, METHOD_DATA *);
typedef void            (*pa_release_f)(void *);

struct patype {
    int type;
    char *name;
    int flags;
#define PA_F_ANNOUNCE		1
#define PA_F_CONFIG		2
#define PA_F_FAST		4 /* available inside FAST */
#define PA_F_NOT_FAST		8 /* only available without FAST */
    size_t pa_ctx_size;
    pa_salt_info_f salt_info;
    /**
     * Return 0 if the PA-mechanism is available and optionally set pa_ctx pointer to non-NULL.
     */
    pa_configure_f configure;
    /**
     * Return 0 if the PA-mechanism can be restarted (time skew, referrals, etc)
     */
    pa_restart_f restart;
    /**
     * Return 0 if the when complete, KRB5_KDC_ERR_MORE_PREAUTH_DATA_REQUIRED if more steps are require
     */
    pa_step_f step;
    pa_release_f release;
} patypes[] = {
    {
	KRB5_PADATA_PK_AS_REP,
	"PKINIT(IETF)",
	PA_F_FAST | PA_F_NOT_FAST,
	0,
	NULL,
	pkinit_configure,
	NULL,
	pkinit_step,
	pkinit_release
    },
    {
	KRB5_PADATA_PK_AS_REP_19,
	"PKINIT(win)",
	PA_F_FAST | PA_F_NOT_FAST,
	0,
	NULL,
	pkinit_configure,
	NULL,
	pkinit_step_win,
	pkinit_release
    },
#ifdef __APPLE_PRIVATE__
    {
	KRB5_PADATA_SRP,
	"SRP",
	PA_F_FAST | PA_F_NOT_FAST,
	sizeof(struct srp_state_data),
	NULL,
	srp_configure,
	NULL,
	srp_step,
	srp_release
    },
#endif
    {
	KRB5_PADATA_ENCRYPTED_CHALLENGE,
	"ENCRYPTED_CHALLENGE",
	PA_F_FAST,
	0,
	NULL,
	NULL,
	NULL,
	enc_chal_step,
	NULL
    },
    {
	KRB5_PADATA_ENC_TIMESTAMP,
	"ENCRYPTED_TIMESTAMP",
	PA_F_NOT_FAST,
	sizeof(struct enc_ts_context),
	NULL,
	NULL,
	enc_ts_restart,
	enc_ts_step,
	enc_ts_release
    },
    {
	KRB5_PADATA_PA_PAC_REQUEST,
	"PA_PAC_REQUEST",
	PA_F_CONFIG,
	0,
	NULL,
	NULL,
	NULL,
	pa_pac_step,
	NULL
    },
    {
	KRB5_PADATA_PKU2U_NAME,
	"PKU2U-NAME",
	PA_F_CONFIG,
	0,
	NULL,
	NULL,
	NULL,
	pa_pku2u_name_step,
	NULL
    },
    {
	KRB5_PADATA_REQ_ENC_PA_REP,
	"REQ-ENC-PA-REP",
	PA_F_CONFIG,
	0,
	NULL,
	NULL,
	NULL,
	pa_enc_pa_rep_step,
	NULL
    },
    {
	KRB5_PADATA_FX_COOKIE,
	"FX-COOKIE",
	PA_F_CONFIG,
	0,
	NULL,
	NULL,
	NULL,
	pa_fx_cookie_step,
	NULL
    },
#define patype_salt(n, f) { KRB5_PADATA_##n, #n, 0, 0, f, NULL, NULL, NULL, NULL }
    patype_salt(ETYPE_INFO2, pa_etype_info2),
    patype_salt(ETYPE_INFO, pa_etype_info),
    patype_salt(PW_SALT, pa_pw_or_afs3_salt),
    patype_salt(AFS3_SALT, pa_pw_or_afs3_salt),
#undef patype_salt
    /* below are just for pretty printing */
#define patype_info(n) { KRB5_PADATA_##n, #n, 0, 0, NULL, NULL, NULL, NULL, NULL }
    patype_info(AUTHENTICATION_SET),
    patype_info(AUTH_SET_SELECTED),
    patype_info(FX_FAST),
    patype_info(FX_ERROR),
    patype_info(PKINIT_KX),
    patype_info(PK_AS_REQ)
#undef patype_info
};

static const char *
get_pa_type_name(int type)
{
    size_t n;
    for (n = 0; n < sizeof(patypes)/sizeof(patypes[0]); n++) 
	if (type == patypes[n].type)
	    return patypes[n].name;
    return "unknown";
}

/*
 *
 */

struct pa_auth_mech {
    struct heim_base_uniq base;
    struct patype *patype;
    heim_object_t next; /* when doing authentication sets */
    char pactx[1];
};

/*
 *
 */

static struct pa_info_data *
process_pa_info(krb5_context context,
		const krb5_principal client,
		const AS_REQ *asreq,
		struct pa_info_data *paid,
		METHOD_DATA *md)
{
    struct pa_info_data *p = NULL;
    PA_DATA *pa;
    size_t i;

    if (md == NULL)
	return NULL;

    for (i = 0; p == NULL && i < sizeof(patypes)/sizeof(patypes[0]); i++) {
	int idx = 0;

	if (patypes[i].salt_info == NULL)
	    continue;

	pa = krb5_find_padata(md->val, md->len, patypes[i].type, &idx);
	if (pa == NULL)
	    continue;

	paid->salt.salttype = (krb5_salttype)patypes[i].type;
	p = patypes[i].salt_info(context, client, asreq, paid, &pa->padata_value);
    }
    return p;
}

static void
pa_announce(krb5_context context,
	    int types,
	    krb5_init_creds_context ctx,
	    METHOD_DATA *in_md,
	    METHOD_DATA *out_md)
{
    size_t n;

    for (n = 0; n < sizeof(patypes)/sizeof(patypes[0]); n++) {
	if ((patypes[n].flags & types) == 0)
	    continue;

	if (patypes[n].step)
	    patypes[n].step(context, ctx, NULL, NULL, NULL, NULL, NULL, in_md, out_md);
	else
	    krb5_padata_add(context, out_md, patypes[n].type, NULL, 0);
    }
}


static void
mech_release(void *ctx)
{
    struct pa_auth_mech *pa_mech = ctx;
    if (pa_mech->patype->release)
	pa_mech->patype->release((void *)&pa_mech->pactx[0]);
}

static struct pa_auth_mech *
pa_mech_create(krb5_context context, krb5_init_creds_context ctx, int pa_type)
{
    struct pa_auth_mech *pa_mech;
    struct patype *patype = NULL;
    size_t n;

    for (n = 0; patype == NULL && n < sizeof(patypes)/sizeof(patypes[0]); n++) {
	if (patypes[n].type == pa_type)
	    patype = &patypes[n];
    }
    if (patype == NULL)
	return NULL;

    pa_mech = heim_uniq_alloc(sizeof(*pa_mech) - 1 + patype->pa_ctx_size, "heim-pa-mech-ctx", mech_release);
    if (pa_mech == NULL)
	return NULL;

    pa_mech->patype = patype;

    if (pa_mech->patype->configure) {
	krb5_error_code ret;

	ret = pa_mech->patype->configure(context, ctx, &pa_mech->pactx[0]);
	if (ret) {
	    heim_release(pa_mech);
	    return NULL;
	}
    }

    _krb5_debugx(context, 5, "Adding PA mech: %s", patype->name);

    return pa_mech;
}

static void
pa_mech_add(krb5_context context, krb5_init_creds_context ctx, int pa_type)
{
    struct pa_auth_mech *mech;

    mech = pa_mech_create(context, ctx, pa_type);
    if (mech) {
	heim_array_append_value(ctx->available_pa_mechs, mech);
	heim_release(mech);
    }
}

static krb5_error_code
pa_configure(krb5_context context,
	     krb5_init_creds_context ctx,
	     METHOD_DATA *in_md)
{
    ctx->available_pa_mechs = heim_array_create();

    if ((ctx->keyproc || ctx->keyseed || ctx->prompter) && !ctx->pk_init_ctx) {
	pa_mech_add(context, ctx, KRB5_PADATA_SRP);
	pa_mech_add(context, ctx, KRB5_PADATA_ENCRYPTED_CHALLENGE);
	pa_mech_add(context, ctx, KRB5_PADATA_ENC_TIMESTAMP);
    }

    if (ctx->pk_init_ctx) {
	pa_mech_add(context, ctx, KRB5_PADATA_PK_AS_REP);
	pa_mech_add(context, ctx, KRB5_PADATA_PK_AS_REP_19);
    }

    /* XXX setup context based on KDC reply */

    return 0;
}
	     
static krb5_error_code
pa_restart(krb5_context context,
	   krb5_init_creds_context ctx)
{
    krb5_error_code ret = HEIM_ERR_PA_CANT_CONTINUE;

    if (ctx->pa_mech && ctx->pa_mech->patype->restart)
	ret = ctx->pa_mech->patype->restart(context, ctx, (void *)&ctx->pa_mech->pactx[0]);

    return ret;
}


static krb5_error_code
pa_step(krb5_context context,
	krb5_init_creds_context ctx,
	const AS_REQ *a,
	const AS_REP *rep,
	const krb5_krbhst_info *hi,
	METHOD_DATA *in_md,
	METHOD_DATA *out_md)
{
    krb5_error_code ret;
    PA_DATA *pa = NULL;
    int idx;

    //heim_assert(in_md != NULL, "pa_step w/o input padata");

 next:
    do {
	if (ctx->pa_mech == NULL) {
	    size_t len = heim_array_get_length(ctx->available_pa_mechs);
	    if (len == 0) {
		_krb5_debugx(context, 0, "no more available_pa_mechs to try");
		return HEIM_ERR_NO_MORE_PA_MECHS;
	    }

	    ctx->pa_mech = heim_array_copy_value(ctx->available_pa_mechs, 0);
	    heim_array_delete_value(ctx->available_pa_mechs, 0);
	}

	if (ctx->fast_state.armor_crypto) {
	    if ((ctx->pa_mech->patype->flags & PA_F_FAST) == 0) {
		_krb5_debugx(context, 0, "pa-mech %s dropped under FAST (not supported)",
			     ctx->pa_mech->patype->name);
		heim_release(ctx->pa_mech);
		ctx->pa_mech = NULL;
		continue;
	    }
	} else {
	    if ((ctx->pa_mech->patype->flags & PA_F_NOT_FAST) == 0) {
		_krb5_debugx(context, 0, "dropped pa-mech %s since not running under FAST",
			     ctx->pa_mech->patype->name);
		heim_release(ctx->pa_mech);
		ctx->pa_mech = NULL;
		continue;
	    }
	}

	_krb5_debugx(context, 0, "pa-mech trying: %s, searching for %d",
		     ctx->pa_mech->patype->name, ctx->pa_mech->patype->type);

	idx = 0;
	if (in_md)
	    pa = krb5_find_padata(in_md->val, in_md->len, ctx->pa_mech->patype->type, &idx);
	else
	    pa = NULL;

    } while (ctx->pa_mech == NULL);

    _krb5_debugx(context, 5, "Stepping pa-mech: %s", ctx->pa_mech->patype->name);

    ret = ctx->pa_mech->patype->step(context, ctx, (void *)&ctx->pa_mech->pactx[0], pa, a, rep, hi, in_md, out_md);
    _krb5_debug(context, 10, ret, "PA type %s returned %d", ctx->pa_mech->patype->name, ret);
    if (ret == 0) {
	struct pa_auth_mech *next_pa = ctx->pa_mech->next;

	if (next_pa) {
	    _krb5_debugx(context, 5, "Next PA type in set is: %s",
			 next_pa->patype->name);
	    ret = HEIM_ERR_PA_CONTINUE_NEEDED;
	} else if (rep == NULL) {
	    _krb5_debugx(context, 5, "PA %s done, but no ticket in sight!!!",
			 ctx->pa_mech->patype->name);
	    ret = HEIM_ERR_PA_CANT_CONTINUE;
	}

	heim_retain(next_pa);
	heim_release(ctx->pa_mech);
	ctx->pa_mech = next_pa;
    }

    if (ret == HEIM_ERR_PA_CANT_CONTINUE) {
	_krb5_debugx(context, 5, "Dropping PA type %s", ctx->pa_mech->patype->name);
	heim_release(ctx->pa_mech);
	ctx->pa_mech = NULL;
	goto next;
    } else if (ret == HEIM_ERR_PA_CONTINUE_NEEDED) {
	_krb5_debugx(context, 5, "Continue needed for %s", ctx->pa_mech->patype->name);
    } else if (ret != 0) {
	_krb5_debugx(context, 5, "Other error from mech %s: %d", ctx->pa_mech->patype->name, ret);
    }

    return ret;
}

static void
log_kdc_pa_types(krb5_context context, METHOD_DATA *in_md)
{
    if (_krb5_have_debug(context, 5)) {
	unsigned i;
	_krb5_debugx(context, 5, "KDC sent %d patypes", in_md->len);
	for (i = 0; i < in_md->len; i++)
	    _krb5_debugx(context, 5, "KDC sent PA-DATA type: %d (%s)",
			 in_md->val[i].padata_type,
			 get_pa_type_name(in_md->val[i].padata_type));
    }
}

/*
 * Assumes caller always will free `out_md', even on error.
 */

static krb5_error_code
process_pa_data_to_md(krb5_context context,
		      const krb5_creds *creds,
		      int first,
		      const AS_REQ *a,
		      krb5_init_creds_context ctx,
		      METHOD_DATA *in_md,
		      METHOD_DATA **out_md)
{
    krb5_error_code ret;

    ALLOC(*out_md, 1);
    if (*out_md == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    (*out_md)->len = 0;
    (*out_md)->val = NULL;

    log_kdc_pa_types(context, in_md);

    if (!first) {
	ret = pa_step(context, ctx, a, NULL, NULL, in_md, *out_md);
	if (ret == HEIM_ERR_PA_CONTINUE_NEEDED) {
	    ret = 0;
	} else if (ret == 0) {
	    _krb5_debugx(context, 0, "pamech done step");
	} else {
	    return ret;
	}
    }

    /*
     * Send announcement (what we support) and configuration (user
     * introduced behavior change)
     */

    pa_announce(context, PA_F_ANNOUNCE|PA_F_CONFIG, ctx, in_md, *out_md);

    /*
     *
     */

    if ((*out_md)->len == 0) {
	free(*out_md);
	*out_md = NULL;
    }

    return 0;
}

static krb5_error_code
process_pa_data_to_key(krb5_context context,
		       krb5_init_creds_context ctx,
		       krb5_creds *creds,
		       AS_REQ *a,
		       AS_REP *rep,
		       const krb5_krbhst_info *hi,
		       krb5_keyblock **key)
{
    struct pa_info_data paid, *ppaid = NULL;
    krb5_error_code ret;
    krb5_enctype etype;

    memset(&paid, 0, sizeof(paid));

    if (rep->padata)
	log_kdc_pa_types(context, rep->padata);

    etype = rep->enc_part.etype;

    if (rep->padata) {
	paid.etype = etype;
	ppaid = process_pa_info(context, creds->client, a, &paid,
				rep->padata);
    }
    if (ppaid == NULL ) {
	if (ctx->paid.etype == KRB5_ENCTYPE_NULL) {
	    ctx->paid.etype = etype;
	    ctx->paid.s2kparams = NULL;
	    ret = krb5_get_pw_salt (context, creds->client, &ctx->paid.salt);
	    if (ret)
		return ret;
	}
	ppaid = &ctx->paid;
    }

    ret = pa_step(context, ctx, a, rep, hi, rep->padata, NULL);
    if (ret == HEIM_ERR_PA_CONTINUE_NEEDED) {
	_krb5_debugx(context, 0, "In final stretch and pa require more stepping ?");
	return ret;
    } else if (ret == 0) {
	_krb5_debugx(context, 0, "final pamech done step");
	goto out;
    } else {
	return ret;
    }
 out:
    free_paid(context, &paid);
    return ret;
}

/*
 *
 */

static krb5_error_code
capture_lkdc_domain(krb5_context context,
		    krb5_init_creds_context ctx)
{
    size_t len;

    len = strlen(_krb5_wellknown_lkdc);

    if (ctx->kdc_hostname != NULL ||
	strncmp(ctx->cred.client->realm, _krb5_wellknown_lkdc, len) != 0 ||
	ctx->cred.client->realm[len] != ':')
	return 0;

    ctx->kdc_hostname = strdup(&ctx->cred.client->realm[len + 1]);

    _krb5_debugx(context, 5, "krb5_get_init_creds: setting LKDC hostname to: %s",
		ctx->kdc_hostname);
    return 0;
}

/**
 * Start a new context to get a new initial credential.
 *
 * @param context A Kerberos 5 context.
 * @param client The Kerberos principal to get the credential for, if
 *     NULL is given, the default principal is used as determined by
 *     krb5_get_default_principal().
 * @param prompter prompter to use if needed
 * @param prompter_data data passed to prompter function
 * @param start_time the time the ticket should start to be valid or 0 for now.
 * @param options a options structure, can be NULL for default options.
 * @param rctx A new allocated free with krb5_init_creds_free().
 *
 * @return 0 for success or an Kerberos 5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_init(krb5_context context,
		     krb5_principal client,
		     krb5_prompter_fct prompter,
		     void *prompter_data,
		     krb5_deltat start_time,
		     krb5_get_init_creds_opt *options,
		     krb5_init_creds_context *rctx)
{
    krb5_init_creds_context ctx;
    krb5_error_code ret;

    *rctx = NULL;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    ret = get_init_creds_common(context, client, start_time, options, ctx);
    if (ret) {
	free(ctx);
	return ret;
    }

    /* Set a new nonce. */
    krb5_generate_random_block (&ctx->nonce, sizeof(ctx->nonce));
    ctx->nonce &= 0x7fffffff;
    /* XXX these just needs to be the same when using Windows PK-INIT */
    ctx->pk_nonce = ctx->nonce;

    ctx->prompter = prompter;
    ctx->prompter_data = prompter_data;

    /* pick up hostname from LKDC realm name */
    ret = capture_lkdc_domain(context, ctx);
    if (ret) {
	free_init_creds_ctx(context, ctx);
	return ret;
    }

    ctx->runflags.allow_enc_pa_rep = 1;

    ctx->fast_state.flags |= KRB5_FAST_AS_REQ;

    *rctx = ctx;

    return ret;
}

/**
 * The set KDC hostname for the initial request, it will not be
 * considered in referrals to another KDC.
 *
 * @param context a Kerberos 5 context.
 * @param ctx a krb5_init_creds_context context.
 * @param hostname the hostname for the KDC of realm
 *
 * @return 0 for success, or an Kerberos 5 error code, see krb5_get_error_message().
 * @ingroup krb5_credential
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_init_creds_set_kdc_hostname(krb5_context context,
				 krb5_init_creds_context ctx,
				 const char *hostname)
{
    if (ctx->kdc_hostname)
	free(ctx->kdc_hostname);
    ctx->kdc_hostname = strdup(hostname);
    if (ctx->kdc_hostname == NULL)
	return ENOMEM;
    return 0;
}
    
/**
 * Sets the service that the is requested. This call is only neede for
 * special initial tickets, by default the a krbtgt is fetched in the default realm.
 *
 * @param context a Kerberos 5 context.
 * @param ctx a krb5_init_creds_context context.
 * @param service the service given as a string, for example
 *        "kadmind/admin". If NULL, the default krbtgt in the clients
 *        realm is set.
 *
 * @return 0 for success, or an Kerberos 5 error code, see krb5_get_error_message().
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_set_service(krb5_context context,
			    krb5_init_creds_context ctx,
			    const char *service)
{
    krb5_const_realm client_realm;
    krb5_principal principal;
    krb5_error_code ret;

    client_realm = krb5_principal_get_realm (context, ctx->cred.client);

    if (service) {
	ret = krb5_parse_name (context, service, &principal);
	if (ret)
	    return ret;
	krb5_principal_set_realm (context, principal, client_realm);
    } else {
	ret = krb5_make_principal(context, &principal,
				  client_realm, KRB5_TGS_NAME, client_realm,
				  NULL);
	if (ret)
	    return ret;
    }

    /*
     * This is for Windows RODC that are picky about what name type
     * the server principal have, and the really strange part is that
     * they are picky about the AS-REQ name type and not the TGS-REQ
     * later. Oh well.
     */

    if (krb5_principal_is_krbtgt(context, principal))
	krb5_principal_set_type(context, principal, KRB5_NT_SRV_INST);

    krb5_free_principal(context, ctx->cred.server);
    ctx->cred.server = principal;

    return 0;
}

/**
 * Sets the password that will use for the request.
 *
 * @param context a Kerberos 5 context.
 * @param ctx a krb5_init_creds_context context.
 * @param cert client cert
 *
 * @return 0 for success, or an Kerberos 5 error code, see krb5_get_error_message().
 * @ingroup krb5_credential
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_init_creds_set_pkinit_client_cert(krb5_context context,
				       krb5_init_creds_context ctx,
				       struct hx509_cert_data *cert)
{
#ifdef PKINIT
    if (ctx->client_cert)
	hx509_cert_free(ctx->client_cert);
    ctx->client_cert = hx509_cert_ref(cert);
    return 0;
#else
    return EINVAL;
#endif
}

/**
 * Sets the password that will use for the request.
 *
 * @param context a Kerberos 5 context.
 * @param ctx krb5_init_creds_context context.
 * @param password the password to use.
 *
 * @return 0 for success, or an Kerberos 5 error code, see krb5_get_error_message().
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_set_password(krb5_context context,
			     krb5_init_creds_context ctx,
			     const char *password)
{
    if (ctx->password) {
	memset(ctx->password, 0, strlen(ctx->password));
	free(ctx->password);
    }
    if (password) {
	ctx->password = strdup(password);
	if (ctx->password == NULL) {
	    krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	    return ENOMEM;
	}
	ctx->keyseed = (void *) ctx->password;
    } else {
	ctx->keyseed = NULL;
	ctx->password = NULL;
    }

    return 0;
}

static krb5_error_code KRB5_CALLCONV
keytab_key_proc(krb5_context context, krb5_enctype enctype,
		krb5_const_pointer keyseed,
		krb5_salt salt, krb5_data *s2kparms,
		krb5_keyblock **key)
{
    krb5_keytab_key_proc_args *args  = rk_UNCONST(keyseed);
    krb5_keytab keytab = args->keytab;
    krb5_principal principal = args->principal;
    krb5_error_code ret;
    krb5_keytab real_keytab;
    krb5_keytab_entry entry;

    if (keytab == NULL) {
	ret = krb5_kt_default(context, &real_keytab);
	if (ret)
	    return ret;
    } else
	real_keytab = keytab;

    ret = krb5_kt_get_entry (context, real_keytab, principal,
			     0, enctype, &entry);

    if (keytab == NULL)
	krb5_kt_close (context, real_keytab);

    if (ret)
	return ret;

    ret = krb5_copy_keyblock (context, &entry.keyblock, key);
    krb5_kt_free_entry(context, &entry);
    return ret;
}


/**
 * Set the keytab to use for authentication.
 *
 * @param context a Kerberos 5 context.
 * @param ctx ctx krb5_init_creds_context context.
 * @param keytab the keytab to read the key from.
 *
 * @return 0 for success, or an Kerberos 5 error code, see krb5_get_error_message().
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_set_keytab(krb5_context context,
			   krb5_init_creds_context ctx,
			   krb5_keytab keytab)
{
    krb5_keytab_key_proc_args *a;
    krb5_keytab_entry entry;
    krb5_kt_cursor cursor;
    krb5_enctype *etypes = NULL;
    krb5_error_code ret;
    size_t netypes = 0;
    int kvno = 0, found = 0;

    a = malloc(sizeof(*a));
    if (a == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    a->principal = ctx->cred.client;
    a->keytab    = keytab;

    ctx->keytab_data = a;
    ctx->keyseed = (void *)a;
    ctx->keyproc = keytab_key_proc;

    /*
     * We need to the KDC what enctypes we support for this keytab,
     * esp if the keytab is really a password based entry, then the
     * KDC might have more enctypes in the database then what we have
     * in the keytab.
     */

    ret = krb5_kt_start_seq_get(context, keytab, &cursor);
    if(ret)
	goto out;

    while(krb5_kt_next_entry(context, keytab, &entry, &cursor) == 0){
	void *ptr;

	if (!krb5_principal_compare(context, entry.principal, ctx->cred.client))
	    goto next;

	found = 1;

	/* check if we ahve this kvno already */
	if (entry.vno > kvno) {
	    /* remove old list of etype */
	    if (etypes)
		free(etypes);
	    etypes = NULL;
	    netypes = 0;
	    kvno = entry.vno;
	} else if (entry.vno != kvno)
	    goto next;

	/* check if enctype is supported */
	if (krb5_enctype_valid(context, entry.keyblock.keytype) != 0)
	    goto next;

	/* add enctype to supported list */
	ptr = realloc(etypes, sizeof(etypes[0]) * (netypes + 2));
	if (ptr == NULL) {
	    free(etypes);
	    ret = krb5_enomem(context);
	    goto out;
	}

	etypes = ptr;
	etypes[netypes] = entry.keyblock.keytype;
	etypes[netypes + 1] = ETYPE_NULL;
	netypes++;
    next:
	krb5_kt_free_entry(context, &entry);
    }
    krb5_kt_end_seq_get(context, keytab, &cursor);

    if (etypes) {
	if (ctx->etypes)
	    free(ctx->etypes);
	ctx->etypes = etypes;
    }

 out:
    if (!found) {
	if (ret == 0)
	    ret = KRB5_KT_NOTFOUND;
	_krb5_kt_principal_not_found(context, ret, keytab, ctx->cred.client, 0, 0);
    }

    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
_krb5_init_creds_set_pku2u(krb5_context context,
			   krb5_init_creds_context ctx,
			   krb5_data *data)
{
#ifdef PKINIT
    krb5_error_code ret;

    ctx->ic_flags |= KRB5_INIT_CREDS_PKU2U;
    ctx->flags = int2KDCOptions(0);

    if (ctx->pku2u_assertion)
	krb5_free_data(context, ctx->pku2u_assertion);

    if (data) {
	ret = krb5_copy_data(context, data, &ctx->pku2u_assertion);
	if (ret)
	    return ret;
    } else {
	ctx->pku2u_assertion = NULL;
    }

    return 0;
#else
    return EINVAL;
#endif
}			     


static krb5_error_code KRB5_CALLCONV
keyblock_key_proc(krb5_context context, krb5_enctype enctype,
		  krb5_const_pointer keyseed,
		  krb5_salt salt, krb5_data *s2kparms,
		  krb5_keyblock **key)
{
    return krb5_copy_keyblock (context, keyseed, key);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_set_keyblock(krb5_context context,
			     krb5_init_creds_context ctx,
			     krb5_keyblock *keyblock)
{
    ctx->keyseed = (void *)keyblock;
    ctx->keyproc = keyblock_key_proc;

    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_set_fast_ccache(krb5_context context,
				krb5_init_creds_context ctx,
				krb5_ccache fast_ccache)
{
    krb5_creds *cred = NULL;
    krb5_error_code ret;
    krb5_data data;

    ret = _krb5_get_krbtgt(context, fast_ccache, NULL, &cred);
    if (ret)
	return ret;

    ret = krb5_cc_get_config(context, fast_ccache, cred->server,
			     "fast_avail", &data);
    krb5_free_creds(context, cred);
    if (ret == 0) {
	ctx->fast_state.armor_ccache = fast_ccache;
	ctx->fast_state.flags |= KRB5_FAST_REQUIRED;
    } else {
	krb5_set_error_message(context, EINVAL, N_("FAST not available for the KDC in the armor ccache", ""));
	return EINVAL;
    }
    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_set_fast_ap_armor_service(krb5_context context,
					  krb5_init_creds_context ctx,
					  krb5_const_principal armor_service)
{
    krb5_error_code ret;

    if (ctx->fast_state.armor_service)
	krb5_free_principal(context, ctx->fast_state.armor_service);
    if (armor_service) {
	ret = krb5_copy_principal(context, armor_service, &ctx->fast_state.armor_service);
	if (ret)
	    return ret;
    } else {
	ctx->fast_state.armor_service = NULL;
    }
    ctx->fast_state.flags |= KRB5_FAST_AP_ARMOR_SERVICE;
    return 0;
}

static krb5_error_code
validate_pkinit_fx(krb5_context context,
		   krb5_init_creds_context ctx,
		   AS_REP *rep,
		   krb5_keyblock *ticket_sessionkey)
{
    krb5_crypto reply_crypto = NULL, kxkey_crypto = NULL;
    krb5_keyblock kxkey, sessionkey;
    krb5_error_code ret;
    EncryptedData ed;
    krb5_data data, pepper1, pepper2;
    PA_DATA *pa = NULL;
    size_t size;
    int idx = 0;

    krb5_keyblock_zero(&sessionkey);
    krb5_keyblock_zero(&kxkey);

    if (rep->padata)
	pa = krb5_find_padata(rep->padata->val, rep->padata->len, KRB5_PADATA_PKINIT_KX, &idx);
    
    if (pa == NULL) {
	if (ctx->flags.request_anonymous && ctx->pk_init_ctx) {
	    /* XXX handle the case where pkinit is not used */
	    krb5_set_error_message(context, KRB5_KDCREP_MODIFIED, N_("Requested anonymous with PKINIT and KDC didn't set PKINIT_KX", ""));
	    return KRB5_KDCREP_MODIFIED;
	}

	return 0;
    }

    heim_assert(ctx->fast_state.reply_key != NULL, "must have a reply key at this stage");

    ret = krb5_crypto_init(context, ctx->fast_state.reply_key, 0, &reply_crypto);
    if (ret)
	goto out;

    ret = decode_EncryptedData(pa->padata_value.data,
			       pa->padata_value.length,
			       &ed, &size);
    if (ret)
	goto out;
    
    ret = krb5_decrypt_EncryptedData(context,
				     reply_crypto,
				     KRB5_KU_PA_PKINIT_KX,
				     &ed,
				     &data);
    free_EncryptedData(&ed);

    ret = decode_EncryptionKey(data.data, data.length, &kxkey, &size);
    if (ret)
	goto out;

    ret = krb5_crypto_init(context, &kxkey, 0, &kxkey_crypto);
    if (ret)
	goto out;

    pepper1.data = "PKINIT";
    pepper1.length = strlen(pepper1.data);

    /*
     * MIT uses KEYEXCHANGE, RFC-6112 say KeyExchange, use what MIT
     * uses since that is what we agreed to on <kitten@ietf.org>.
     */
    pepper2.data = "KEYEXCHANGE";
    pepper2.length = strlen(pepper2.data);

    ret = krb5_crypto_fx_cf2(context, kxkey_crypto, reply_crypto,
			     &pepper1, &pepper2, kxkey.keytype,
			     &sessionkey);
    if (ret)
	goto out;

    if (sessionkey.keytype != ticket_sessionkey->keytype ||
	krb5_data_ct_cmp(&sessionkey.keyvalue, &ticket_sessionkey->keyvalue) != 0)
    {
	ret = KRB5_KDCREP_MODIFIED;
	krb5_set_error_message(context, ret, N_("PKINIT-KX session key doesn't match", ""));
	goto out;
    }

    ctx->ic_flags |= KRB5_INIT_CREDS_PKINIT_KX_VALID;

 out:
    krb5_free_keyblock_contents(context, &kxkey);
    krb5_free_keyblock_contents(context, &sessionkey);
    if (reply_crypto)
	krb5_crypto_destroy(context, reply_crypto);
    if (kxkey_crypto)
	krb5_crypto_destroy(context, kxkey_crypto);

    return ret;
}

/**
 * The core loop if krb5_get_init_creds() function family. Create the
 * packets and have the caller send them off to the KDC.
 *
 * If the caller want all work been done for them, use
 * krb5_init_creds_get() instead.
 *
 * @param context a Kerberos 5 context.
 * @param ctx ctx krb5_init_creds_context context.
 * @param in input data from KDC, first round it should be reset by krb5_data_zer().
 * @param out reply to KDC.
 * @param hostinfo KDC address info, first round it can be NULL.
 * @param realm realm is a pointer the realm to send the packet, it have the
 *        lifetime the next call to krb5_init_creds_step() or krb5_init_creds_free().
 * @param flags status of the round, if
 *        KRB5_INIT_CREDS_STEP_FLAG_CONTINUE is set, continue one more round.
 *
 * @return 0 for success, or an Kerberos 5 error code, see
 *     krb5_get_error_message().
 *
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_step(krb5_context context,
		     krb5_init_creds_context ctx,
		     krb5_data *in,
		     krb5_data *out,
		     krb5_krbhst_info *hostinfo,
		     krb5_realm *realm,
		     unsigned int *flags)
{
    struct timeval start_time, end_time;
    krb5_data checksum_data;
    krb5_error_code ret;
    size_t len = 0;
    size_t size;
    AS_REQ req2;
    int first = 0;

    gettimeofday(&start_time, NULL);

    krb5_data_zero(out);
    krb5_data_zero(&checksum_data);
    
    if (realm)
	*realm = NULL;

    if (ctx->as_req.req_body.cname == NULL) {

#ifdef PKINIT
	if (ctx->pk_init_ctx && ctx->client_cert)
	    _krb5_pk_set_user_id(context, ctx->pk_init_ctx, ctx->client_cert);
#endif

	ret = init_as_req(context, ctx->flags, &ctx->cred,
			  ctx->addrs, ctx->etypes, &ctx->as_req);
	if (ret) {
	    free_init_creds_ctx(context, ctx);
	    return ret;
	}
	if (ctx->fast_state.flags & KRB5_FAST_REQUIRED)
	    ;
	else if (ctx->fast_state.flags & KRB5_FAST_AP_ARMOR_SERVICE)
	    /* Check with armor service if there is FAST */;
	else
	    ctx->fast_state.flags |= KRB5_FAST_DISABLED;


	/* XXX should happen after we get back reply from KDC */
	pa_configure(context, ctx, NULL);

	first = 1;
    }

#define MAX_PA_COUNTER 10
    if (ctx->pa_counter > MAX_PA_COUNTER) {
	krb5_set_error_message(context, KRB5_GET_IN_TKT_LOOP,
			       N_("Looping %d times while getting "
				  "initial credentials", ""),
			       ctx->pa_counter);
	return KRB5_GET_IN_TKT_LOOP;
    }
    ctx->pa_counter++;

    _krb5_debugx(context, 5, "krb5_get_init_creds: loop %d", ctx->pa_counter);

    /* Lets process the input packet */
    if (in && in->length) {
	krb5_kdc_rep rep;

	memset(&rep, 0, sizeof(rep));

	_krb5_debugx(context, 5, "krb5_get_init_creds: processing input");

	ret = decode_AS_REP(in->data, in->length, &rep.kdc_rep, &size);
	if (ret == 0) {
	    unsigned eflags = EXTRACT_TICKET_AS_REQ | EXTRACT_TICKET_TIMESYNC;
	    krb5_data data;

	    /*
	     * Unwrap AS-REP
	     */
	    ASN1_MALLOC_ENCODE(Ticket, data.data, data.length,
			       &rep.kdc_rep.ticket, &size, ret);
	    if (ret)
		goto out;
	    heim_assert(data.length == size, "ASN.1 internal error");

	    ret = _krb5_fast_unwrap_kdc_rep(context, ctx->nonce, &data,
					    &ctx->fast_state, &rep.kdc_rep);
	    krb5_data_free(&data);
	    if (ret)
		goto out;

	    /*
	     * Now check and extract the ticket
	     */

	    if (ctx->flags.canonicalize) {
		eflags |= EXTRACT_TICKET_ALLOW_SERVER_MISMATCH;
		eflags |= EXTRACT_TICKET_MATCH_REALM;
	    }
	    if (ctx->ic_flags & KRB5_INIT_CREDS_NO_C_CANON_CHECK)
		eflags |= EXTRACT_TICKET_ALLOW_CNAME_MISMATCH;

	    ret = process_pa_data_to_key(context, ctx, &ctx->cred,
					 &ctx->as_req, &rep.kdc_rep,
					 hostinfo, &ctx->fast_state.reply_key);
	    if (ret) {
		free_AS_REP(&rep.kdc_rep);
		goto out;
	    }

	    if (ctx->fast_state.strengthen_key) {
		krb5_keyblock result;

		_krb5_debugx(context, 5, "krb5_get_init_creds: FAST strengthen_key");

		ret = _krb5_fast_cf2(context,
				     ctx->fast_state.strengthen_key,
				     "strengthenkey", 
				     ctx->fast_state.reply_key,
				     "replykey",
				     &result,
				     NULL);
		if (ret) {
		    free_AS_REP(&rep.kdc_rep);
		    goto out;
		}
		
		krb5_free_keyblock_contents(context, ctx->fast_state.reply_key);
		*ctx->fast_state.reply_key = result;
	    }

	    _krb5_debugx(context, 5, "krb5_get_init_creds: extracting ticket");

	    ret = _krb5_extract_ticket(context,
				       &rep,
				       &ctx->cred,
				       ctx->fast_state.reply_key,
				       KRB5_KU_AS_REP_ENC_PART,
				       NULL,
				       ctx->nonce,
				       eflags,
				       &ctx->req_buffer,
				       NULL,
				       NULL);

	    if (ret == 0)
		ret = copy_EncKDCRepPart(&rep.enc_part, &ctx->enc_part);
	    if (ret == 0)
		ret = validate_pkinit_fx(context, ctx, &rep.kdc_rep, &ctx->cred.session);

	    krb5_free_keyblock(context, ctx->fast_state.reply_key);
	    ctx->fast_state.reply_key = NULL;
	    ctx->ic_flags |= KRB5_INIT_CREDS_DONE;
	    *flags = 0;

	    free_AS_REP(&rep.kdc_rep);
	    free_EncASRepPart(&rep.enc_part);

	    gettimeofday(&end_time, NULL);
	    timevalsub(&end_time, &start_time);
	    timevaladd(&ctx->stats.run_time, &end_time);

	    _krb5_debugx(context, 1, "krb5_get_init_creds: wc: %ld.%06d",
			 ctx->stats.run_time.tv_sec, ctx->stats.run_time.tv_usec);
	    return ret;

	} else {
	    /* let's try to parse it as a KRB-ERROR */

	    _krb5_debugx(context, 5, "krb5_get_init_creds: got an KRB-ERROR from KDC");

	    free_KRB_ERROR(&ctx->error);

	    ret = krb5_rd_error(context, in, &ctx->error);
	    if(ret && in->length && ((char*)in->data)[0] == 4)
		ret = KRB5KRB_AP_ERR_V4_REPLY;
	    if (ret) {
		_krb5_debugx(context, 5, "krb5_get_init_creds: failed to read error");
		goto out;
	    }
	    
	    /*
	     * Unwrap method-data, if there is any,
	     * fast_unwrap_error() below might replace it with a
	     * wrapped version if we are using FAST.
	     */

	    free_METHOD_DATA(&ctx->md);
	    memset(&ctx->md, 0, sizeof(ctx->md));

	    if (ctx->error.e_data) {
		krb5_error_code ret2;

		ret2 = decode_METHOD_DATA(ctx->error.e_data->data,
					 ctx->error.e_data->length,
					 &ctx->md,
					 NULL);
		if (ret) {
		    /*
		     * Just ignore any error, the error will be pushed
		     * out from krb5_error_from_rd_error() if there
		     * was one.
		     */
		    _krb5_debug(context, 5, ret, N_("Failed to decode METHOD-DATA", ""));
		}
	    }

	    /*
	     * Unwrap KRB-ERROR, we are always calling this so that
	     * FAST can tell us if your peer KDC suddenly dropped FAST
	     * wrapping and its really an attacker's packet (or a bug
	     * in the KDC).
	     */
	    ret = _krb5_fast_unwrap_error(context, &ctx->fast_state,
					  &ctx->md, &ctx->error);
	    if (ret)
		goto out;

	    /*
	     *
	     */

	    ret = krb5_error_from_rd_error(context, &ctx->error, &ctx->cred);

	    /* log the failure */
	    if (_krb5_have_debug(context, 5)) {
		const char *str = krb5_get_error_message(context, ret);
		_krb5_debugx(context, 5, "krb5_get_init_creds: KRB-ERROR %d/%s", ret, str);
		krb5_free_error_message(context, str);
	    }

	    /*
	     * Handle special error codes
	     */

	    if (ret == KRB5KDC_ERR_PREAUTH_REQUIRED
		|| ret == KRB5_KDC_ERR_MORE_PREAUTH_DATA_REQUIRED
		|| ret == KRB5KDC_ERR_ETYPE_NOSUPP)
	    {
		/*
		 * If no preauth was set and KDC requires it, give it one
		 * more try.
		 *
		 * If the KDC returned KRB5KDC_ERR_ETYPE_NOSUPP, just loop
		 * one more time since that might mean we are dealing with
		 * a Windows KDC that is confused about what enctypes are
		 * available.
		 */

		if (ctx->md.len == 0) {
		    krb5_set_error_message(context, ret,
					   N_("Preauth required but no preauth "
					      "options send by KDC", ""));
		    goto out;
		}
	    } else if (ret == KRB5KRB_AP_ERR_SKEW && context->kdc_sec_offset == 0) {
		/*
		 * Try adapt to timeskrew when we are using pre-auth, and
		 * if there was a time skew, try again.
		 */
		krb5_set_real_time(context, ctx->error.stime, -1);
		if (context->kdc_sec_offset)
		    ret = 0;

		_krb5_debugx(context, 10, "init_creds: err skew updateing kdc offset to %d",
			    context->kdc_sec_offset);
		if (ret)
		    goto out;

		pa_restart(context, ctx);

		first = 1;

	    } else if (ret == KRB5_KDC_ERR_WRONG_REALM && ctx->flags.canonicalize) {
	        /* client referal to a new realm */
		char *ref_realm;

		if (ctx->error.crealm == NULL) {
		    krb5_set_error_message(context, ret,
					   N_("Got a client referral, not but no realm", ""));
		    goto out;
		}
		ref_realm = *ctx->error.crealm;

		_krb5_debugx(context, 5, "krb5_get_init_creds: referral to realm %s",
			    ref_realm);

		/*
		 * If its a krbtgt, lets updat the requested krbtgt too
		 */
		if (krb5_principal_is_krbtgt(context, ctx->cred.server)) {

		    free(ctx->cred.server->name.name_string.val[1]);
		    ctx->cred.server->name.name_string.val[1] = strdup(ref_realm);
		    if (ctx->cred.server->name.name_string.val[1] == NULL) {
			ret = ENOMEM;
			goto out;
		    }

		    free_PrincipalName(ctx->as_req.req_body.sname);
		    ret = _krb5_principal2principalname(ctx->as_req.req_body.sname, ctx->cred.server);
		    if (ret)
			goto out;
		}

		free(ctx->as_req.req_body.realm);
		ret = copy_Realm(&ref_realm, &ctx->as_req.req_body.realm);
		if (ret)
		    goto out;

		ret = krb5_principal_set_realm(context,
					       ctx->cred.client,
					       *ctx->error.crealm);
		if (ret)
		    goto out;

		ret = krb5_unparse_name(context, ctx->cred.client, &ref_realm);
		if (ret == 0) {
		    _krb5_debugx(context, 5, "krb5_get_init_creds: got referal to %s", ref_realm);
		    krb5_xfree(ref_realm);
		}

		pa_restart(context, ctx);

		first = 1;

	    } else if (ret == KRB5KDC_ERR_KEY_EXP && ctx->runflags.change_password == 0 && ctx->prompter) {
		char buf2[1024];
		
		ctx->runflags.change_password = 1;
		
		ctx->prompter(context, ctx->prompter_data, NULL, N_("Password has expired", ""), 0, NULL);
		

		/* try to avoid recursion */
		if (ctx->in_tkt_service != NULL && strcmp(ctx->in_tkt_service, "kadmin/changepw") == 0)
		    goto out;
		
		/* don't include prompter in runtime */
		gettimeofday(&end_time, NULL);
		timevalsub(&end_time, &start_time);
		timevaladd(&ctx->stats.run_time, &end_time);

		ret = change_password(context,
				      ctx->cred.client,
				      ctx->password,
				      buf2,
				      sizeof(buf2),
				      ctx->prompter,
				      ctx->prompter_data,
				      NULL);
		if (ret)
		    goto out;
		
		gettimeofday(&start_time, NULL);

		krb5_init_creds_set_password(context, ctx, buf2);

		pa_restart(context, ctx);

		first = 1;

	    } else if (ret == KRB5KDC_ERR_PREAUTH_FAILED) {

		/*
		 * Old MIT KDC can't handle KRB5_PADATA_REQ_ENC_PA_REP,
		 * so drop it and try again. But only try that for MIT 
		 * Kerberos servers by keying of no METHOD-DATA.
		 */
		if (ctx->runflags.allow_enc_pa_rep) {
		    if (ctx->md.len != 0) {
			_krb5_debugx(context, 10, "Server send PA data with KRB-ERROR, "
				     "so not a pre 1.7 MIT KDC and wont retry w/o ENC-PA-REQ");
			goto out;
		    }		    
		    _krb5_debugx(context, 10, "Disable allow_enc_pa_rep and trying again");
		    ctx->runflags.allow_enc_pa_rep = 0;
		    goto retry;
		}

		if (ctx->fast_state.flags & KRB5_FAST_DISABLED) {
		    _krb5_debugx(context, 10, "FAST disabled and got preauth failed");
		    goto out;
		}

		if ((ctx->fast_state.flags & KRB5_FAST_OPTIMISTIC) == 0) {
		    _krb5_debugx(context, 10, "Preauth failed");
		    goto out;
		}

		_krb5_debugx(context, 10, "preauth failed with Optimistic FAST, trying w/o FAST");

		ctx->fast_state.flags &= ~KRB5_FAST_OPTIMISTIC;
		ctx->fast_state.flags |= KRB5_FAST_DISABLED;

	    retry:
		pa_restart(context, ctx);

		first = 1;

	    } else {
		if (ctx->fast_state.flags & KRB5_FAST_OPTIMISTIC) {
		    _krb5_debugx(context, 10,
				 "Some other error %d failed with Optimistic FAST, trying w/o FAST", ret);

		    ctx->fast_state.flags &= ~KRB5_FAST_OPTIMISTIC;
		    ctx->fast_state.flags |= KRB5_FAST_DISABLED;
		    pa_restart(context, ctx);

		    first = 1;
		} else {
		    /* some other error code from the KDC, lets return it to the user */
		    goto out;
		}
	    }
	}
    }

    if (ctx->as_req.padata) {
	free_METHOD_DATA(ctx->as_req.padata);
	free(ctx->as_req.padata);
	ctx->as_req.padata = NULL;
    }

    ret = _krb5_fast_create_armor(context, &ctx->fast_state,
				  ctx->cred.client->realm);
    if (ret)
	goto out;

    /* Set a new nonce. */
    ctx->as_req.req_body.nonce = ctx->nonce;


    /*
     * Step and announce PA-DATA
     */

    ret = process_pa_data_to_md(context, &ctx->cred, first, &ctx->as_req, ctx,
				&ctx->md, &ctx->as_req.padata);
    if (ret)
	goto out;

	
    /*
     * Wrap with FAST
     */
    ret = copy_AS_REQ(&ctx->as_req, &req2);
    if (ret)
	goto out;

    ret = _krb5_fast_wrap_req(context,
			      &ctx->fast_state,
			      NULL,
			      &req2);

    krb5_data_free(&checksum_data);
    if (ret) {
	free_AS_REQ(&req2);
	goto out;
    }

    krb5_data_free(&ctx->req_buffer);

    ASN1_MALLOC_ENCODE(AS_REQ,
		       ctx->req_buffer.data, ctx->req_buffer.length,
		       &req2, &len, ret);
    free_AS_REQ(&req2);
    if (ret)
	goto out;
    if(len != ctx->req_buffer.length)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    out->data = ctx->req_buffer.data;
    out->length = ctx->req_buffer.length;

    *flags = KRB5_INIT_CREDS_STEP_FLAG_CONTINUE;

    if (realm)
	*realm = ctx->cred.client->realm;
    

    gettimeofday(&end_time, NULL);
    timevalsub(&end_time, &start_time);
    timevaladd(&ctx->stats.run_time, &end_time);

    return 0;
 out:
    return ret;
}

/**
 * Extract the newly acquired credentials from krb5_init_creds_context
 * context.
 *
 * @param context A Kerberos 5 context.
 * @param ctx ctx krb5_init_creds_context context.
 * @param cred credentials, free with krb5_free_cred_contents().
 *
 * @return 0 for sucess or An Kerberos error code, see krb5_get_error_message().
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_get_creds(krb5_context context,
			  krb5_init_creds_context ctx,
			  krb5_creds *cred)
{
    return krb5_copy_creds_contents(context, &ctx->cred, cred);
}

KRB5_LIB_FUNCTION krb5_timestamp KRB5_LIB_CALL
_krb5_init_creds_get_cred_endtime(krb5_context context, krb5_init_creds_context ctx)
{
    return ctx->cred.times.endtime;
}

KRB5_LIB_FUNCTION krb5_principal KRB5_LIB_CALL
_krb5_init_creds_get_cred_client(krb5_context context, krb5_init_creds_context ctx)
{
    return ctx->cred.client;
}



/**
 * Get the last error from the transaction.
 *
 * @return Returns 0 or an error code
 *
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_get_error(krb5_context context,
			  krb5_init_creds_context ctx,
			  KRB_ERROR *error)
{
    krb5_error_code ret;

    ret = copy_KRB_ERROR(&ctx->error, error);
    if (ret)
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));

    return ret;
}

/**
 * Store config
 *
 * @param context A Kerberos 5 context.
 * @param ctx The krb5_init_creds_context to free.
 * @param id store 
 *
 * @return Returns 0 or an error code
 *
 * @ingroup krb5_credential
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_init_creds_store_config(krb5_context context,
			     krb5_init_creds_context ctx,
			     krb5_ccache id)
{
    krb5_error_code ret;

    if (ctx->kdc_hostname) {
	krb5_data data;
	data.length = strlen(ctx->kdc_hostname);
	data.data = ctx->kdc_hostname;

	ret = krb5_cc_set_config(context, id, NULL, "lkdc-hostname", &data);
	if (ret)
	    return ret;
    }
    return 0;
}


krb5_error_code
krb5_init_creds_store(krb5_context context,
		      krb5_init_creds_context ctx,
		      krb5_ccache id)
{
    krb5_error_code ret;

    if (ctx->cred.client == NULL) {
	ret = KRB5KDC_ERR_PREAUTH_REQUIRED;
	krb5_set_error_message(context, ret, "init creds not completed yet");
	return ret;
    }

    ret = krb5_cc_initialize(context, id, ctx->cred.client);
    if (ret)
	return ret;

    ret = krb5_cc_store_cred(context, id, &ctx->cred);
    if (ret)
	return ret;

    if (ctx->cred.flags.b.enc_pa_rep) {
	krb5_data data = { 3, rk_UNCONST("yes") };
	ret = krb5_cc_set_config(context, id, ctx->cred.server,
				 "fast_avail", &data);
	if (ret)
	    return ret;
    }

    return 0;
}

/**
 * Free the krb5_init_creds_context allocated by krb5_init_creds_init().
 *
 * @param context A Kerberos 5 context.
 * @param ctx The krb5_init_creds_context to free.
 *
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_init_creds_free(krb5_context context,
		     krb5_init_creds_context ctx)
{
    free_init_creds_ctx(context, ctx);
    free(ctx);
}

/**
 * Get new credentials as setup by the krb5_init_creds_context.
 *
 * @param context A Kerberos 5 context.
 * @param ctx The krb5_init_creds_context to process.
 *
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_init_creds_get(krb5_context context, krb5_init_creds_context ctx)
{
    krb5_sendto_ctx stctx = NULL;
    krb5_krbhst_info *hostinfo = NULL;
    krb5_error_code ret;
    krb5_data in, out;
    unsigned int flags = 0;

    krb5_data_zero(&in);
    krb5_data_zero(&out);

    ret = krb5_sendto_ctx_alloc(context, &stctx);
    if (ret)
	goto out;
    krb5_sendto_ctx_set_func(stctx, _krb5_kdc_retry, NULL);

    if (ctx->kdc_hostname)
	krb5_sendto_set_hostname(context, stctx, ctx->kdc_hostname);

    while (1) {
	krb5_realm realm = NULL;
	struct timeval nstart, nend;
	
	flags = 0;
	ret = krb5_init_creds_step(context, ctx, &in, &out, hostinfo, &realm, &flags);
	krb5_data_free(&in);
	if (ret)
	    goto out;

	if ((flags & KRB5_INIT_CREDS_STEP_FLAG_CONTINUE) == 0)
	    break;

	gettimeofday(&nstart, NULL);

	ret = krb5_sendto_context (context, stctx, &out, realm, &in);
    	if (ret)
	    goto out;

	gettimeofday(&nend, NULL);
	timevalsub(&nend, &nstart);
	timevaladd(&ctx->stats.run_time, &nend);
    }

 out:
    if (stctx)
	krb5_sendto_ctx_free(context, stctx);

    return ret;
}

/**
 * Get new credentials using password.
 *
 * @ingroup krb5_credential
 */


KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_init_creds_password(krb5_context context,
			     krb5_creds *creds,
			     krb5_principal client,
			     const char *password,
			     krb5_prompter_fct prompter,
			     void *data,
			     krb5_deltat start_time,
			     const char *in_tkt_service,
			     krb5_get_init_creds_opt *options)
{
    krb5_init_creds_context ctx;
    char buf[BUFSIZ];
    krb5_error_code ret;
    int chpw = 0;

 again:
    ret = krb5_init_creds_init(context, client, prompter, data, start_time, options, &ctx);
    if (ret)
	goto out;

    ret = krb5_init_creds_set_service(context, ctx, in_tkt_service);
    if (ret)
	goto out;

    if (prompter != NULL && ctx->password == NULL && password == NULL) {
	krb5_prompt prompt;
	krb5_data password_data;
	char *p, *q;

	krb5_unparse_name (context, client, &p);
	asprintf (&q, "%s's Password: ", p);
	free (p);
	prompt.prompt = q;
	password_data.data   = buf;
	password_data.length = sizeof(buf);
	prompt.hidden = 1;
	prompt.reply  = &password_data;
	prompt.type   = KRB5_PROMPT_TYPE_PASSWORD;

	ret = (*prompter) (context, data, NULL, NULL, 1, &prompt);
	free (q);
	if (ret) {
	    memset (buf, 0, sizeof(buf));
	    ret = KRB5_LIBOS_PWDINTR;
	    krb5_clear_error_message (context);
	    goto out;
	}
	password = password_data.data;
    }

    if (password) {
	ret = krb5_init_creds_set_password(context, ctx, password);
	if (ret)
	    goto out;
    }

    ret = krb5_init_creds_get(context, ctx);

    if (ret == 0)
	krb5_process_last_request(context, options, ctx);

    if (ret == KRB5KDC_ERR_KEY_EXPIRED && chpw == 0) {
	char buf2[1024];

	/* try to avoid recursion */
	if (in_tkt_service != NULL && strcmp(in_tkt_service, "kadmin/changepw") == 0)
	   goto out;

	/* don't try to change password where then where none */
	if (prompter == NULL)
	    goto out;

	ret = change_password (context,
			       client,
			       ctx->password,
			       buf2,
			       sizeof(buf2),
			       prompter,
			       data,
			       options);
	if (ret)
	    goto out;
	chpw = 1;
	krb5_init_creds_free(context, ctx);

	goto again;
    }

 out:
    if (ret == 0)
	krb5_init_creds_get_creds(context, ctx, creds);

    if (ctx)
	krb5_init_creds_free(context, ctx);

    memset(buf, 0, sizeof(buf));
    return ret;
}

/**
 * Get new credentials using keyblock.
 *
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_init_creds_keyblock(krb5_context context,
			     krb5_creds *creds,
			     krb5_principal client,
			     krb5_keyblock *keyblock,
			     krb5_deltat start_time,
			     const char *in_tkt_service,
			     krb5_get_init_creds_opt *options)
{
    krb5_init_creds_context ctx;
    krb5_error_code ret;

    memset(creds, 0, sizeof(*creds));

    ret = krb5_init_creds_init(context, client, NULL, NULL, start_time, options, &ctx);
    if (ret)
	goto out;

    ret = krb5_init_creds_set_service(context, ctx, in_tkt_service);
    if (ret)
	goto out;

    ret = krb5_init_creds_set_keyblock(context, ctx, keyblock);
    if (ret)
	goto out;

    ret = krb5_init_creds_get(context, ctx);

    if (ret == 0)
        krb5_process_last_request(context, options, ctx);

 out:
    if (ret == 0)
	krb5_init_creds_get_creds(context, ctx, creds);

    if (ctx)
	krb5_init_creds_free(context, ctx);

    return ret;
}

/**
 * Get new credentials using keytab.
 *
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_init_creds_keytab(krb5_context context,
			   krb5_creds *creds,
			   krb5_principal client,
			   krb5_keytab keytab,
			   krb5_deltat start_time,
			   const char *in_tkt_service,
			   krb5_get_init_creds_opt *options)
{
    krb5_init_creds_context ctx;
    krb5_error_code ret;

    memset(creds, 0, sizeof(*creds));

    ret = krb5_init_creds_init(context, client, NULL, NULL, start_time, options, &ctx);
    if (ret)
	goto out;

    ret = krb5_init_creds_set_service(context, ctx, in_tkt_service);
    if (ret)
	goto out;

    ret = krb5_init_creds_set_keytab(context, ctx, keytab);
    if (ret)
	goto out;

    ret = krb5_init_creds_get(context, ctx);
    if (ret == 0)
        krb5_process_last_request(context, options, ctx);

 out:
    if (ret == 0)
	krb5_init_creds_get_creds(context, ctx, creds);

    if (ctx)
	krb5_init_creds_free(context, ctx);

    return ret;
}
