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
#include <assert.h>

/*
 * Take the `body' and encode it into `padata' using the credentials
 * in `creds'.
 */

static krb5_error_code
make_pa_tgs_req(krb5_context context,
		krb5_auth_context ac,
		KDC_REQ_BODY *body,
		PA_DATA *padata,
		krb5_ccache ccache,
		krb5_creds *creds)
{
    u_char *buf;
    size_t buf_size;
    size_t len = 0;
    krb5_data in_data;
    krb5_error_code ret;

    ASN1_MALLOC_ENCODE(KDC_REQ_BODY, buf, buf_size, body, &len, ret);
    if (ret)
	goto out;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    in_data.length = len;
    in_data.data   = buf;
    ret = _krb5_mk_req_internal(context, &ac, 0, &in_data, ccache, creds,
				&padata->padata_value,
				KRB5_KU_TGS_REQ_AUTH_CKSUM,
				KRB5_KU_TGS_REQ_AUTH);
 out:
    free (buf);
    if(ret)
	return ret;
    padata->padata_type = KRB5_PADATA_TGS_REQ;
    return 0;
}

/*
 * Set the `enc-authorization-data' in `req_body' based on `authdata'
 */

static krb5_error_code
set_auth_data (krb5_context context,
	       KDC_REQ_BODY *req_body,
	       krb5_authdata *authdata,
	       krb5_keyblock *subkey)
{
    if(authdata->len) {
	size_t len = 0, buf_size;
	unsigned char *buf;
	krb5_crypto crypto;
	krb5_error_code ret;

	ASN1_MALLOC_ENCODE(AuthorizationData, buf, buf_size, authdata,
			   &len, ret);
	if (ret)
	    return ret;
	if (buf_size != len)
	    krb5_abortx(context, "internal error in ASN.1 encoder");

	ALLOC(req_body->enc_authorization_data, 1);
	if (req_body->enc_authorization_data == NULL) {
	    free (buf);
	    krb5_set_error_message(context, ENOMEM,
				   N_("malloc: out of memory", ""));
	    return ENOMEM;
	}
	ret = krb5_crypto_init(context, subkey, 0, &crypto);
	if (ret) {
	    free (buf);
	    free (req_body->enc_authorization_data);
	    req_body->enc_authorization_data = NULL;
	    return ret;
	}
	krb5_encrypt_EncryptedData(context,
				   crypto,
				   KRB5_KU_TGS_REQ_AUTH_DAT_SUBKEY,
				   buf,
				   len,
				   0,
				   req_body->enc_authorization_data);
	free (buf);
	krb5_crypto_destroy(context, crypto);
    } else {
	req_body->enc_authorization_data = NULL;
    }
    return 0;
}

/*
 * Create a tgs-req in `t' with `addresses', `flags', `second_ticket'
 * (if not-NULL), `in_creds', `krbtgt', and returning the generated
 * subkey in `subkey'.
 */

krb5_error_code
_krb5_init_tgs_req(krb5_context context,
		   krb5_ccache ccache,
		   krb5_addresses *addresses,
		   krb5_kdc_flags flags,
		   krb5_const_principal impersonate_principal,
		   Ticket *second_ticket,
		   krb5_creds *in_creds,
		   krb5_creds *krbtgt,
		   unsigned nonce,
		   METHOD_DATA *padata,
		   krb5_keyblock **subkey,
		   TGS_REQ *t)
{
    krb5_auth_context ac = NULL;
    krb5_error_code ret = 0;
    
    /* inherit the forwardable/proxyable flags from the krbtgt */
    flags.b.forwardable = krbtgt->flags.b.forwardable;
    flags.b.proxiable = krbtgt->flags.b.proxiable;

    if (ccache->ops->tgt_req) {
	KERB_TGS_REQ_OUT out;
	KERB_TGS_REQ_IN in;
	
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	ret = ccache->ops->tgt_req(context, ccache, &in, &out);
	if (ret)
	    return ret;

	free_KERB_TGS_REQ_OUT(&out);
	return 0;
    }


    memset(t, 0, sizeof(*t));

    if (impersonate_principal) {
	krb5_crypto crypto;
	PA_S4U2Self self;
	krb5_data data;
	void *buf;
	size_t size, len;

	self.name = impersonate_principal->name;
	self.realm = impersonate_principal->realm;
	self.auth = rk_UNCONST("Kerberos");
	
	ret = _krb5_s4u2self_to_checksumdata(context, &self, &data);
	if (ret)
	    goto fail;

	ret = krb5_crypto_init(context, &krbtgt->session, 0, &crypto);
	if (ret) {
	    krb5_data_free(&data);
	    goto fail;
	}

	ret = krb5_create_checksum(context,
				   crypto,
				   KRB5_KU_OTHER_CKSUM,
				   0,
				   data.data,
				   data.length,
				   &self.cksum);
	krb5_crypto_destroy(context, crypto);
	krb5_data_free(&data);
	if (ret)
	    goto fail;

	ASN1_MALLOC_ENCODE(PA_S4U2Self, buf, len, &self, &size, ret);
	free_Checksum(&self.cksum);
	if (ret)
	    goto fail;
	if (len != size)
	    krb5_abortx(context, "internal asn1 error");
	
	ret = krb5_padata_add(context, padata, KRB5_PADATA_FOR_USER, buf, len);
	if (ret)
	    goto fail;
    }

    t->pvno = 5;
    t->msg_type = krb_tgs_req;
    if (in_creds->session.keytype) {
	ALLOC_SEQ(&t->req_body.etype, 1);
	if(t->req_body.etype.val == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret,
				   N_("malloc: out of memory", ""));
	    goto fail;
	}
	t->req_body.etype.val[0] = in_creds->session.keytype;
    } else {
	ret = _krb5_init_etype(context,
			       KRB5_PDU_TGS_REQUEST,
			       &t->req_body.etype.len,
			       &t->req_body.etype.val,
			       NULL);
    }
    if (ret)
	goto fail;
    t->req_body.addresses = addresses;
    t->req_body.kdc_options = flags.b;
    ret = copy_Realm(&in_creds->server->realm, &t->req_body.realm);
    if (ret)
	goto fail;
    ALLOC(t->req_body.sname, 1);
    if (t->req_body.sname == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	goto fail;
    }

    /* some versions of some code might require that the client be
       present in TGS-REQs, but this is clearly against the spec */

    ret = copy_PrincipalName(&in_creds->server->name, t->req_body.sname);
    if (ret)
	goto fail;

    /* req_body.till should be NULL if there is no endtime specified,
       but old MIT code (like DCE secd) doesn't like that */
    ALLOC(t->req_body.till, 1);
    if(t->req_body.till == NULL){
	ret = ENOMEM;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	goto fail;
    }
    *t->req_body.till = in_creds->times.endtime;

    t->req_body.nonce = nonce;
    if(second_ticket){
	ALLOC(t->req_body.additional_tickets, 1);
	if (t->req_body.additional_tickets == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret,
				   N_("malloc: out of memory", ""));
	    goto fail;
	}
	ALLOC_SEQ(t->req_body.additional_tickets, 1);
	if (t->req_body.additional_tickets->val == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret,
				   N_("malloc: out of memory", ""));
	    goto fail;
	}
	ret = copy_Ticket(second_ticket, t->req_body.additional_tickets->val);
	if (ret)
	    goto fail;
    }
    ALLOC(t->padata, 1);
    if (t->padata == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	goto fail;
    }
    ALLOC_SEQ(t->padata, 1 + padata->len);
    if (t->padata->val == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	goto fail;
    }
    {
	size_t i;
	for (i = 0; i < padata->len; i++) {
	    ret = copy_PA_DATA(&padata->val[i], &t->padata->val[i + 1]);
	    if (ret) {
		krb5_set_error_message(context, ret,
				       N_("malloc: out of memory", ""));
		goto fail;
	    }
	}
    }

    ret = krb5_auth_con_init(context, &ac);
    if(ret)
	goto fail;

    ret = krb5_auth_con_generatelocalsubkey(context, ac, &krbtgt->session);
    if (ret)
	goto fail;

    ret = set_auth_data (context, &t->req_body, &in_creds->authdata,
			 ac->local_subkey);
    if (ret)
	goto fail;

    ret = make_pa_tgs_req(context,
			  ac,
			  &t->req_body,
			  &t->padata->val[0],
			  ccache,
			  krbtgt);
    if(ret)
	goto fail;

    ret = krb5_auth_con_getlocalsubkey(context, ac, subkey);
    if (ret)
	goto fail;

fail:
    if (ac)
	krb5_auth_con_free(context, ac);
    if (ret) {
	t->req_body.addresses = NULL;
	free_TGS_REQ (t);
    }
    return ret;
}

/* DCE compatible decrypt proc */
krb5_error_code KRB5_CALLCONV
_krb5_decrypt_tkt_with_subkey (krb5_context context,
			       krb5_keyblock *key,
			       krb5_key_usage usage,
			       krb5_const_pointer skey,
			       krb5_kdc_rep *dec_rep)
{
    const krb5_keyblock *subkey = skey;
    krb5_error_code ret = 0;
    krb5_data data;
    size_t size;
    krb5_crypto crypto;

    assert(usage == 0);

    krb5_data_zero(&data);

    /*
     * start out with trying with subkey if we have one
     */
    if (subkey) {
	ret = krb5_crypto_init(context, subkey, 0, &crypto);
	if (ret)
	    return ret;
	ret = krb5_decrypt_EncryptedData (context,
					  crypto,
					  KRB5_KU_TGS_REP_ENC_PART_SUB_KEY,
					  &dec_rep->kdc_rep.enc_part,
					  &data);
	/*
	 * If the is Windows 2000 DC, we need to retry with key usage
	 * 8 when doing ARCFOUR.
	 */
	if (ret && subkey->keytype == ETYPE_ARCFOUR_HMAC_MD5) {
	    ret = krb5_decrypt_EncryptedData(context,
					     crypto,
					     8,
					     &dec_rep->kdc_rep.enc_part,
					     &data);
	}
	krb5_crypto_destroy(context, crypto);
    }
    if (subkey == NULL || ret) {
	ret = krb5_crypto_init(context, key, 0, &crypto);
	if (ret)
	    return ret;
	ret = krb5_decrypt_EncryptedData (context,
					  crypto,
					  KRB5_KU_TGS_REP_ENC_PART_SESSION,
					  &dec_rep->kdc_rep.enc_part,
					  &data);
	krb5_crypto_destroy(context, crypto);
    }
    if (ret)
	return ret;

    ret = decode_EncASRepPart(data.data,
			      data.length,
			      &dec_rep->enc_part,
			      &size);
    if (ret)
	ret = decode_EncTGSRepPart(data.data,
				   data.length,
				   &dec_rep->enc_part,
				   &size);
    if (ret)
      krb5_set_error_message(context, ret,
			     N_("Failed to decode encpart in ticket", ""));
    krb5_data_free (&data);
    return ret;
}

static int
not_found(krb5_context context, krb5_const_principal p, krb5_error_code code)
{
    krb5_error_code ret;
    char *str;

    ret = krb5_unparse_name(context, p, &str);
    if(ret) {
	krb5_clear_error_message(context);
	return code;
    }
    krb5_set_error_message(context, code,
			   N_("Matching credential (%s) not found", ""), str);
    free(str);
    return code;
}

static krb5_error_code
find_cred(krb5_context context,
	  krb5_ccache id,
	  krb5_principal server,
	  krb5_creds **tgts,
	  krb5_creds *out_creds)
{
    krb5_error_code ret;
    krb5_creds mcreds;

    krb5_cc_clear_mcred(&mcreds);
    mcreds.server = server;
    ret = krb5_cc_retrieve_cred(context, id, KRB5_TC_DONT_MATCH_REALM,
				&mcreds, out_creds);
    if(ret == 0)
	return 0;
    while(tgts && *tgts){
	if(krb5_compare_creds(context, KRB5_TC_DONT_MATCH_REALM,
			      &mcreds, *tgts)){
	    ret = krb5_copy_creds_contents(context, *tgts, out_creds);
	    return ret;
	}
	tgts++;
    }
    return not_found(context, server, KRB5_CC_NOTFOUND);
}

static krb5_error_code
add_cred(krb5_context context, krb5_creds const *tkt, krb5_creds ***tgts)
{
    int i;
    krb5_error_code ret;
    krb5_creds **tmp = *tgts;

    for(i = 0; tmp && tmp[i]; i++); /* XXX */
    tmp = realloc(tmp, (i+2)*sizeof(*tmp));
    if(tmp == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    *tgts = tmp;
    ret = krb5_copy_creds(context, tkt, &tmp[i]);
    tmp[i+1] = NULL;
    return ret;
}

/*
 * Store all credentials that seems not to be _the_ krbtgt
 * credential.
 */

static void
store_tgts(krb5_context context, krb5_ccache ccache, krb5_creds **tgts)
{
    size_t n;

    for (n = 0; tgts && tgts[n]; n++) {
	krb5_const_principal server = tgts[n]->server; 

	if (krb5_principal_is_krbtgt(context, server) && strcmp(server->name.name_string.val[1], server->realm) != 0)
	    krb5_cc_store_cred(context, ccache, tgts[n]);
    }
    for (n = 0; tgts && tgts[n]; n++)
	krb5_free_creds(context, tgts[n]);
}

/*
 *
 */

typedef krb5_error_code
(*tkt_step_state)(krb5_context, krb5_tkt_creds_context,
		  krb5_data *, krb5_data *, krb5_realm *, unsigned int *);


struct krb5_tkt_creds_context_data {
    struct heim_base_uniq base;
    krb5_context context;
    tkt_step_state state;
    unsigned int options;
    char *server_name;
    krb5_error_code error;

    /* input data */
    krb5_kdc_flags req_kdc_flags;
    krb5_principal impersonate_principal; /* s4u2(self) */
    krb5_addresses *addreseses;
    krb5_ccache ccache;
    krb5_creds *in_cred;

    /* current tgs state, reset/free with tkt_reset() */
    krb5_kdc_flags kdc_flags;
    int32_t nonce;
    krb5_keyblock *subkey;
    krb5_creds tgt;
    krb5_creds next; /* next name we are going to fetch */
    krb5_creds **tickets;
    int ok_as_delegate;

    /* output */
    krb5_creds *cred;
};

#define TKT_STEP(funcname)					\
static krb5_error_code tkt_##funcname(krb5_context,		\
		   krb5_tkt_creds_context,			\
		   krb5_data *, krb5_data *, krb5_realm *,	\
		   unsigned int *)

TKT_STEP(init);
TKT_STEP(referral_init);
TKT_STEP(referral_recv);
TKT_STEP(referral_send);
TKT_STEP(direct_init);
TKT_STEP(capath_init);
TKT_STEP(store);
#undef TKT_STEP


/*
 * Setup state to transmit the first request and send the request
 */

static krb5_error_code
tkt_init(krb5_context context,
	 krb5_tkt_creds_context ctx,
	 krb5_data *in,
	 krb5_data *out,
	 krb5_realm *realm,
	 unsigned int *flags)
{
    _krb5_debugx(context, 10, "tkt_init: %s", ctx->server_name);
    ctx->state = tkt_referral_init;
    return 0;
}

static void
tkt_reset(krb5_context context, krb5_tkt_creds_context ctx)
{
    krb5_free_cred_contents(context, &ctx->tgt);
    krb5_free_cred_contents(context, &ctx->next);
    krb5_free_keyblock(context, ctx->subkey);
    ctx->subkey = NULL;
}

/*
 * Get 
 */

static krb5_error_code
tkt_referral_init(krb5_context context,
		  krb5_tkt_creds_context ctx,
		  krb5_data *in,
		  krb5_data *out,
		  krb5_realm *realm,
		  unsigned int *flags)
{
    krb5_error_code ret;
    krb5_creds ticket;
    krb5_const_realm client_realm;
    krb5_principal tgtname;

    _krb5_debugx(context, 10, "tkt_step_referrals: %s", ctx->server_name);

    memset(&ticket, 0, sizeof(ticket));
    memset(&ctx->kdc_flags, 0, sizeof(ctx->kdc_flags));
    memset(&ctx->next, 0, sizeof(ctx->next));

    ctx->kdc_flags.b.canonicalize = 1;

    client_realm = krb5_principal_get_realm(context, ctx->in_cred->client);

    /* find tgt for the clients base realm */
    ret = krb5_make_principal(context, &tgtname,
			      client_realm,
			      KRB5_TGS_NAME,
			      client_realm,
			      NULL);
    if(ret)
	goto out;
	
    ret = find_cred(context, ctx->ccache, tgtname, NULL, &ctx->tgt);
    krb5_free_principal(context, tgtname);
    if (ret)
	goto out;


    ret = krb5_copy_principal(context, ctx->in_cred->client, &ctx->next.client);
    if (ret)
	goto out;

    ret = krb5_copy_principal(context, ctx->in_cred->server, &ctx->next.server);
    if (ret)
	goto out;

    ret = krb5_principal_set_realm(context, ctx->next.server, ctx->tgt.server->realm);
    if (ret)
	goto out;


 out:
    if (ret) {
	ctx->error = ret;
	ctx->state = tkt_direct_init;
    } else {
	ctx->error = 0;
	ctx->state = tkt_referral_send;
    }

    return 0;
}

static krb5_error_code
tkt_referral_send(krb5_context context,
		  krb5_tkt_creds_context ctx,
		  krb5_data *in,
		  krb5_data *out,
		  krb5_realm *realm,
		  unsigned int *flags)
{
    krb5_error_code ret;
    TGS_REQ req;
    size_t len;
    METHOD_DATA padata;

    padata.val = NULL;
    padata.len = 0;

    krb5_generate_random_block(&ctx->nonce, sizeof(ctx->nonce));
    ctx->nonce &= 0xffffffff;

    if (_krb5_have_debug(context, 10)) {
	char *sname, *tgtname;
	krb5_unparse_name(context, ctx->tgt.server, &tgtname);
	krb5_unparse_name(context, ctx->next.server, &sname);
	_krb5_debugx(context, 10, "sending TGS-REQ for %s using %s", sname, tgtname);
    }

    ret = _krb5_init_tgs_req(context,
			     ctx->ccache,
			     ctx->addreseses,
			     ctx->kdc_flags,
			     ctx->impersonate_principal,
			     NULL,
			     &ctx->next,
			     &ctx->tgt,
			     ctx->nonce,
			     &padata,
			     &ctx->subkey,
			     &req);
    if (ret)
	goto out;

    ASN1_MALLOC_ENCODE(TGS_REQ, out->data, out->length, &req, &len, ret);
    if (ret)
	goto out;
    if(out->length != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    /* don't free addresses */
    req.req_body.addresses = NULL;
    free_TGS_REQ(&req);

    *realm = ctx->tgt.server->name.name_string.val[1];

    *flags |= KRB5_TKT_STATE_CONTINUE;
    
    ctx->error = 0;
    ctx->state = tkt_referral_recv;

    return 0;
    
out:
    ctx->error = ret;
    ctx->state = NULL;
    return ret;
}

static krb5_error_code
parse_tgs_rep(krb5_context context,
	      krb5_tkt_creds_context ctx,
	      krb5_data *in,
	      krb5_creds *outcred)
{
    krb5_error_code ret;
    krb5_kdc_rep rep;
    size_t len;
    
    memset(&rep, 0, sizeof(rep));
    memset(outcred, 0, sizeof(*outcred));

    if (ctx->ccache->ops->tgt_rep) {
	return EINVAL;
    }

    if(decode_TGS_REP(in->data, in->length, &rep.kdc_rep, &len) == 0) {
	unsigned eflags = 0;
	
	ret = krb5_copy_principal(context,
				  ctx->next.client,
				  &outcred->client);
	if(ret)
	    return ret;
	ret = krb5_copy_principal(context,
				  ctx->next.server,
				  &outcred->server);
	if(ret)
	    return ret;
	/* this should go someplace else */
	outcred->times.endtime = ctx->in_cred->times.endtime;
	
	if (ctx->kdc_flags.b.constrained_delegation || ctx->impersonate_principal)
	    eflags |= EXTRACT_TICKET_ALLOW_CNAME_MISMATCH;
	
	ret = _krb5_extract_ticket(context,
				   &rep,
				   outcred,
				   &ctx->tgt.session,
				   0,
				   &ctx->tgt.addresses,
				   ctx->nonce,
				   eflags,
				   NULL,
				   _krb5_decrypt_tkt_with_subkey,
				   ctx->subkey);

    } else if(krb5_rd_error(context, in, &rep.error) == 0) {
	ret = krb5_error_from_rd_error(context, &rep.error, ctx->in_cred);
    } else {
	ret = KRB5KRB_AP_ERR_MSG_TYPE;
	krb5_clear_error_message(context);
    }
    krb5_free_kdc_rep(context, &rep);
    return ret;
}


static krb5_error_code
tkt_referral_recv(krb5_context context,
		  krb5_tkt_creds_context ctx,
		  krb5_data *in,
		  krb5_data *out,
		  krb5_realm *realm,
		  unsigned int *flags)
{
    krb5_error_code ret;
    krb5_creds outcred, mcred;
    unsigned long n;

    _krb5_debugx(context, 10, "tkt_referral_recv: %s", ctx->server_name);
    
    memset(&outcred, 0, sizeof(outcred));

    ret = parse_tgs_rep(context, ctx, in, &outcred);
    if (ret) {
	_krb5_debugx(context, 10, "tkt_referral_recv: parse_tgs_rep %d", ret);
	tkt_reset(context, ctx);
	ctx->state = tkt_capath_init;
	return 0;
    }
    
    /*
     * Check if we found the right ticket
     */
    
    if (krb5_principal_compare_any_realm(context, ctx->next.server, outcred.server)) {
	ret = krb5_copy_creds(context, &outcred, &ctx->cred);
	if (ret)
	    return (ctx->error = ret);
	krb5_free_cred_contents(context, &outcred);
	ctx->state = tkt_store;
	return 0;
    }

    if (!krb5_principal_is_krbtgt(context, outcred.server)) {
	krb5_set_error_message(context, KRB5KRB_AP_ERR_NOT_US,
			       N_("Got back an non krbtgt "
				      "ticket referrals", ""));
	krb5_free_cred_contents(context, &outcred);
	ctx->state = tkt_capath_init;
	return 0;
    }
    
    _krb5_debugx(context, 10, "KDC for realm %s sends a referrals to %s",
		ctx->tgt.server->realm, outcred.server->name.name_string.val[1]);

    /*
     * check if there is a loop
     */
    krb5_cc_clear_mcred(&mcred);
    mcred.server = outcred.server;

    for (n = 0; ctx->tickets && ctx->tickets[n]; n++) {
	if(krb5_compare_creds(context,
			      KRB5_TC_DONT_MATCH_REALM,
			      &mcred,
			      ctx->tickets[n]))
	{
	    _krb5_debugx(context, 5, "Referral from %s loops back to realm %s",
				    ctx->tgt.server->realm,
				    outcred.server->realm);
	    ctx->state = tkt_capath_init;
	    return 0;
	}
    }
#define MAX_KDC_REFERRALS_LOOPS 15
    if (n > MAX_KDC_REFERRALS_LOOPS) {
	ctx->state = tkt_capath_init;
	return 0;
    }

    /*
     * filter out ok-as-delegate if needed
     */
    
    if (ctx->ok_as_delegate == 0 || outcred.flags.b.ok_as_delegate == 0) {
	ctx->ok_as_delegate = 0;
	outcred.flags.b.ok_as_delegate = 0;
    }

    /* add to iteration cache */
    ret = add_cred(context, &outcred, &ctx->tickets);
    if (ret) {
	ctx->state = tkt_capath_init;
	return 0;
    }

    /* set up next server to talk to */
    krb5_free_cred_contents(context, &ctx->tgt);
    ctx->tgt = outcred;
    
    /*
     * Setup next target principal to target
     */

    ret = krb5_principal_set_realm(context, ctx->next.server,
				   ctx->tgt.server->realm);
    if (ret) {
	ctx->state = tkt_capath_init;
	return 0;
    }
    
    ctx->state = tkt_referral_send;
    
    return 0;
}

static krb5_error_code
tkt_direct_init(krb5_context context,
		krb5_tkt_creds_context ctx,
		krb5_data *in,
		krb5_data *out,
		krb5_realm *realm,
		unsigned int *flags)
{
    _krb5_debugx(context, 10, "tkt_direct_init: %s", ctx->server_name);

    tkt_reset(context, ctx);

    ctx->error = EINVAL;
    ctx->state = tkt_capath_init;

    return 0;
}

static krb5_error_code
tkt_capath_init(krb5_context context,
		krb5_tkt_creds_context ctx,
		krb5_data *in,
		krb5_data *out,
		krb5_realm *realm,
		unsigned int *flags)
{
    _krb5_debugx(context, 10, "tkt_step_capath: %s", ctx->server_name);

    tkt_reset(context, ctx);

    ctx->error = EINVAL;
    ctx->state = NULL;

    return 0;
}


static krb5_error_code
tkt_store(krb5_context context,
	  krb5_tkt_creds_context ctx,
	  krb5_data *in,
	  krb5_data *out,
	  krb5_realm *realm,
	  unsigned int *flags)
{
    krb5_boolean bret;

    _krb5_debugx(context, 10, "tkt_step_store: %s", ctx->server_name);

    ctx->error = 0;
    ctx->state = NULL;

    if (ctx->options & KRB5_GC_NO_STORE)
	return 0;

    if (ctx->tickets) {
	store_tgts(context, ctx->ccache, ctx->tickets);
	free(ctx->tickets);
	ctx->tickets = NULL;
    }

    heim_assert(ctx->cred != NULL, "store but no credential");

    krb5_cc_store_cred(context, ctx->ccache, ctx->cred);
    /*
     * Store an referrals entry since the server changed from that
     * expected and if we want to find it again next time, it
     * better have the right name.
     *
     * We only need to compare any realm since the referrals
     * matching code will do the same for us.
     */
    bret = krb5_principal_compare_any_realm(context,
					    ctx->cred->server,
					    ctx->in_cred->server);
    if (!bret) {
	krb5_creds ref = *ctx->cred;
	krb5_principal_data refp = *ctx->in_cred->server;
	refp.realm = "";
	ref.server = &refp;
	krb5_cc_store_cred(context, ctx->ccache, &ref);
    }

    return 0;
}


static void
tkt_release(void *ptr)
{
    krb5_tkt_creds_context ctx = ptr;
    krb5_free_creds(ctx->context, ctx->cred);
    tkt_reset(ctx->context, ctx);
    if (ctx->tickets) {
	size_t n;
	for (n = 0; ctx->tickets[n]; n++)
	    krb5_free_creds(ctx->context, ctx->tickets[n]);
	free(ctx->tickets);
    }
    free(ctx->server_name);
}

/**
 * Create a context for a credential fetching process
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_tkt_creds_init(krb5_context context,
		    krb5_ccache ccache,
                    krb5_creds *in_cred,
		    krb5_flags options,
                    krb5_tkt_creds_context *pctx)
{
    krb5_tkt_creds_context ctx;
    krb5_error_code ret;

    *pctx = NULL;

    ctx = heim_uniq_alloc(sizeof(*ctx), "tkt-ctx", tkt_release);
    if (ctx == NULL)
	return ENOMEM;

    ctx->context = context;
    ctx->state = tkt_init;
    ctx->options = options;
    ctx->ccache = ccache;

    if (ctx->options & KRB5_GC_FORWARDABLE)
	ctx->req_kdc_flags.b.forwardable = 1;
    if (ctx->options & KRB5_GC_USER_USER) {
	ctx->req_kdc_flags.b.enc_tkt_in_skey = 1;
	ctx->options |= KRB5_GC_NO_STORE;
    }
    if (options & KRB5_GC_CANONICALIZE)
	ctx->req_kdc_flags.b.canonicalize = 1;

    ret = krb5_copy_creds(context, in_cred, &ctx->in_cred);
    if (ret) {
	heim_release(ctx);
	return ret;
    }

    ret = krb5_unparse_name(context, ctx->in_cred->server, &ctx->server_name);
    if (ret) {
	heim_release(ctx);
	return ret;
    }

    *pctx = ctx;

    return 0;
}

/**
 * Step though next step in the TGS-REP process
 *
 * Pointer returned in realm is valid to next call to
 * krb5_tkt_creds_step() or krb5_tkt_creds_free().
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_tkt_creds_step(krb5_context context,
		    krb5_tkt_creds_context ctx,
                    krb5_data *in,
		    krb5_data *out,
		    krb5_realm *realm,
                    unsigned int *flags)
{
    krb5_error_code ret;

    krb5_data_zero(out);
    *flags = 0;
    *realm = NULL;

    ret = ctx->error = 0;

    while (out->length == 0 && ctx->state != NULL) {
	ret = ctx->state(context, ctx, in, out, realm, flags);
	if (ret) {
	    heim_assert(ctx->error == ret, "error not same as saved");
	    break;
	}

	if ((*flags) & KRB5_TKT_STATE_CONTINUE) {
	    heim_assert(out->length != 0, "no data to send to KDC");
	    heim_assert(*realm != NULL, "no realm to send data too");
	    break;
	} else {
	    heim_assert(out->length == 0, "out state but not state continue");
	}
    }

    return ret;
}

/**
 *
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_tkt_creds_get_creds(krb5_context context,
			 krb5_tkt_creds_context ctx,
                         krb5_creds **cred)
{
    if (ctx->state != NULL)
	return EINVAL;

    if (ctx->cred)
	return krb5_copy_creds(context, ctx->cred, cred);

    return ctx->error;
}

/**
 *
 */

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_tkt_creds_free(krb5_context context,
		    krb5_tkt_creds_context ctx)
{
    heim_release(ctx);
}
