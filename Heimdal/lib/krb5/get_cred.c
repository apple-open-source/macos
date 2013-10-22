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

static krb5_error_code
get_cred_kdc_capath(krb5_context, krb5_kdc_flags,
		    krb5_ccache, krb5_creds *, krb5_principal,
		    Ticket *, const char *, krb5_creds **, krb5_creds ***);

krb5_error_code
_krb5_get_krbtgt(krb5_context context,
		 krb5_ccache  id,
		 krb5_realm realm,
		 krb5_creds **cred)
{
    krb5_error_code ret;
    krb5_creds tmp_cred;

    memset(&tmp_cred, 0, sizeof(tmp_cred));

    ret = krb5_cc_get_principal(context, id, &tmp_cred.client);
    if (ret)
	return ret;

    ret = krb5_make_principal(context,
			      &tmp_cred.server,
			      realm,
			      KRB5_TGS_NAME,
			      realm,
			      NULL);
    if(ret) {
	krb5_free_principal(context, tmp_cred.client);
	return ret;
    }
    ret = krb5_get_credentials(context,
			       KRB5_GC_CACHED,
			       id,
			       &tmp_cred,
			       cred);
    krb5_free_principal(context, tmp_cred.client);
    krb5_free_principal(context, tmp_cred.server);
    if(ret)
	return ret;
    return 0;
}

static krb5_error_code
get_cred_kdc(krb5_context context,
	     krb5_ccache id,
	     krb5_kdc_flags flags,
	     krb5_addresses *addresses,
	     krb5_creds *in_creds,
	     krb5_creds *krbtgt,
	     krb5_principal impersonate_principal,
	     Ticket *second_ticket,
	     const char *kdc_hostname,
	     krb5_creds *out_creds)
{
    TGS_REQ req;
    krb5_data enc;
    krb5_data resp;
    krb5_kdc_rep rep;
    KRB_ERROR error;
    krb5_error_code ret;
    unsigned nonce;
    krb5_keyblock *subkey = NULL;
    size_t len = 0;
    Ticket second_ticket_data;
    krb5_deltat offset;
    METHOD_DATA padata;

    krb5_data_zero(&resp);
    krb5_data_zero(&enc);
    padata.val = NULL;
    padata.len = 0;

    krb5_generate_random_block(&nonce, sizeof(nonce));
    nonce &= 0xffffffff;

    if(flags.b.enc_tkt_in_skey && second_ticket == NULL){
	ret = decode_Ticket(in_creds->second_ticket.data,
			    in_creds->second_ticket.length,
			    &second_ticket_data, &len);
	if(ret)
	    return ret;
	second_ticket = &second_ticket_data;
    }

    ret = krb5_cc_get_kdc_offset(context, id, &offset);
    if (ret == 0) {
	context->kdc_sec_offset = (uint32_t)offset;
	context->kdc_usec_offset = 0;
    }

    ret = _krb5_init_tgs_req (context,
			      id,
			      addresses,
			      flags,
			      impersonate_principal,
			      second_ticket,
			      in_creds,
			      krbtgt,
			      nonce,
			      &padata,
			      &subkey,
			      &req);
    if (ret)
	goto out;

    ASN1_MALLOC_ENCODE(TGS_REQ, enc.data, enc.length, &req, &len, ret);
    if (ret)
	goto out;
    if(enc.length != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    /* don't free addresses */
    req.req_body.addresses = NULL;
    free_TGS_REQ(&req);

    /*
     * Send and receive
     */
    {
	krb5_sendto_ctx stctx;
	ret = krb5_sendto_ctx_alloc(context, &stctx);
	if (ret)
	    return ret;
	krb5_sendto_ctx_set_func(stctx, _krb5_kdc_retry, NULL);

	if (kdc_hostname)
	    krb5_sendto_set_hostname(context, stctx, kdc_hostname);

	ret = krb5_sendto_context (context, stctx, &enc,
				   krbtgt->server->name.name_string.val[1],
				   &resp);
	krb5_sendto_ctx_free(context, stctx);
    }
    if(ret)
	goto out;

    memset(&rep, 0, sizeof(rep));
    if(decode_TGS_REP(resp.data, resp.length, &rep.kdc_rep, &len) == 0) {
	unsigned eflags = 0;

	ret = krb5_copy_principal(context,
				  in_creds->client,
				  &out_creds->client);
	if(ret)
	    goto out2;
	ret = krb5_copy_principal(context,
				  in_creds->server,
				  &out_creds->server);
	if(ret)
	    goto out2;
	/* this should go someplace else */
	out_creds->times.endtime = in_creds->times.endtime;

	/* XXX should do better testing */
	if (flags.b.constrained_delegation || impersonate_principal)
	    eflags |= EXTRACT_TICKET_ALLOW_CNAME_MISMATCH;

	ret = _krb5_extract_ticket(context,
				   &rep,
				   out_creds,
				   &krbtgt->session,
				   0,
				   &krbtgt->addresses,
				   nonce,
				   eflags,
				   NULL,
				   _krb5_decrypt_tkt_with_subkey,
				   subkey);
    out2:
	krb5_free_kdc_rep(context, &rep);
    } else if(krb5_rd_error(context, &resp, &error) == 0) {
	ret = krb5_error_from_rd_error(context, &error, in_creds);
	krb5_free_error_contents(context, &error);
    } else if(resp.length > 0 && ((char*)resp.data)[0] == 4) {
	ret = KRB5KRB_AP_ERR_V4_REPLY;
	krb5_clear_error_message(context);
    } else {
	ret = KRB5KRB_AP_ERR_MSG_TYPE;
	krb5_clear_error_message(context);
    }

out:
    if (second_ticket == &second_ticket_data)
	free_Ticket(&second_ticket_data);
    free_METHOD_DATA(&padata);
    krb5_data_free(&resp);
    krb5_data_free(&enc);
    if(subkey)
	krb5_free_keyblock(context, subkey);
    return ret;

}

/*
 * same as above, just get local addresses first if the krbtgt have
 * them and the realm is not addressless
 */

static krb5_error_code
get_cred_kdc_address(krb5_context context,
		     krb5_ccache id,
		     krb5_kdc_flags flags,
		     krb5_addresses *addrs,
		     krb5_creds *in_creds,
		     krb5_creds *krbtgt,
		     krb5_principal impersonate_principal,
		     Ticket *second_ticket,
		     const char *kdc_hostname,
		     krb5_creds *out_creds)
{
    krb5_error_code ret;
    krb5_addresses addresses = { 0, NULL };

    /*
     * Inherit the address-ness of the krbtgt if the address is not
     * specified.
     */

    if (addrs == NULL && krbtgt->addresses.len != 0) {
	krb5_boolean noaddr;

	krb5_appdefault_boolean(context, NULL, krbtgt->server->realm,
				"no-addresses", FALSE, &noaddr);

	if (!noaddr) {
	    krb5_get_all_client_addrs(context, &addresses);
	    /* XXX this sucks. */
	    addrs = &addresses;
	    if(addresses.len == 0)
		addrs = NULL;
	}
    }
    ret = get_cred_kdc(context, id, flags, addrs, in_creds,
		       krbtgt, impersonate_principal,
		       second_ticket, kdc_hostname, out_creds);
    krb5_free_addresses(context, &addresses);
    return ret;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_kdc_cred(krb5_context context,
		  krb5_ccache id,
		  krb5_kdc_flags flags,
		  krb5_addresses *addresses,
		  Ticket  *second_ticket,
		  krb5_creds *in_creds,
		  krb5_creds **out_creds
		  )
{
    krb5_error_code ret;
    krb5_creds *krbtgt;

    *out_creds = calloc(1, sizeof(**out_creds));
    if(*out_creds == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    ret = _krb5_get_krbtgt (context,
			    id,
			    in_creds->server->realm,
			    &krbtgt);
    if(ret) {
	free(*out_creds);
	*out_creds = NULL;
	return ret;
    }
    ret = get_cred_kdc(context, id, flags, addresses,
		       in_creds, krbtgt, NULL, NULL, NULL, *out_creds);
    krb5_free_creds (context, krbtgt);
    if(ret) {
	free(*out_creds);
	*out_creds = NULL;
    }
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

static krb5_error_code
get_cred_kdc_capath_worker(krb5_context context,
                           krb5_kdc_flags flags,
                           krb5_ccache ccache,
                           krb5_creds *in_creds,
                           krb5_const_realm try_realm,
                           krb5_principal impersonate_principal,
                           Ticket *second_ticket,
			   const char *kdc_hostname,
                           krb5_creds **out_creds,
                           krb5_creds ***ret_tgts)
{
    krb5_error_code ret;
    krb5_creds *tgt, tmp_creds;
    krb5_const_realm client_realm, server_realm;
    int ok_as_delegate = 1;

    *out_creds = NULL;

    client_realm = krb5_principal_get_realm(context, in_creds->client);
    server_realm = krb5_principal_get_realm(context, in_creds->server);
    memset(&tmp_creds, 0, sizeof(tmp_creds));
    ret = krb5_copy_principal(context, in_creds->client, &tmp_creds.client);
    if(ret)
	return ret;

    ret = krb5_make_principal(context,
			      &tmp_creds.server,
			      try_realm,
			      KRB5_TGS_NAME,
			      server_realm,
			      NULL);
    if(ret){
	krb5_free_principal(context, tmp_creds.client);
	return ret;
    }
    {
	krb5_creds tgts;

	ret = find_cred(context, ccache, tmp_creds.server,
			*ret_tgts, &tgts);
	if(ret == 0){
	    /* only allow implicit ok_as_delegate if the realm is the clients realm */
	    if (strcmp(try_realm, client_realm) != 0 || strcmp(try_realm, server_realm) != 0)
		ok_as_delegate = tgts.flags.b.ok_as_delegate;

	    *out_creds = calloc(1, sizeof(**out_creds));
	    if(*out_creds == NULL) {
		ret = ENOMEM;
		krb5_set_error_message(context, ret,
				       N_("malloc: out of memory", ""));
	    } else {
		ret = get_cred_kdc_address(context, ccache, flags, NULL,
					   in_creds, &tgts,
					   impersonate_principal,
					   second_ticket,
					   kdc_hostname,
					   *out_creds);
		if (ret) {
		    free (*out_creds);
		    *out_creds = NULL;
		} else if (krb5_principal_compare_any_realm(context, (*out_creds)->server, in_creds->server) != TRUE) {
		    krb5_free_creds(context, *out_creds);
		    *out_creds = NULL;
		    ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
		} else if (ok_as_delegate == 0)
		    (*out_creds)->flags.b.ok_as_delegate = 0;

	    }
	    krb5_free_cred_contents(context, &tgts);
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
    }
    if(krb5_realm_compare(context, in_creds->client, in_creds->server))
	return not_found(context, in_creds->server, KRB5_CC_NOTFOUND);

    /* XXX this can loop forever */
    while(1){
	heim_general_string tgt_inst;

	ret = get_cred_kdc_capath(context, flags, ccache, &tmp_creds,
				  NULL, NULL, kdc_hostname, &tgt, ret_tgts);
	if(ret) {
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
	/*
	 * if either of the chain or the ok_as_delegate was stripped
	 * by the kdc, make sure we strip it too.
	 */
	if (ok_as_delegate == 0 || tgt->flags.b.ok_as_delegate == 0) {
	    ok_as_delegate = 0;
	    tgt->flags.b.ok_as_delegate = 0;
	}

	ret = add_cred(context, tgt, ret_tgts);
	if(ret) {
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
	tgt_inst = tgt->server->name.name_string.val[1];
	if(strcmp(tgt_inst, server_realm) == 0)
	    break;
	krb5_free_principal(context, tmp_creds.server);
	ret = krb5_make_principal(context, &tmp_creds.server,
				  tgt_inst, KRB5_TGS_NAME, server_realm, NULL);
	if(ret) {
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
	ret = krb5_free_creds(context, tgt);
	if(ret) {
	    krb5_free_principal(context, tmp_creds.server);
	    krb5_free_principal(context, tmp_creds.client);
	    return ret;
	}
    }

    krb5_free_principal(context, tmp_creds.server);
    krb5_free_principal(context, tmp_creds.client);
    *out_creds = calloc(1, sizeof(**out_creds));
    if(*out_creds == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
    } else {
	ret = get_cred_kdc_address (context, ccache, flags, NULL,
				    in_creds, tgt, impersonate_principal,
				    second_ticket, kdc_hostname, *out_creds);
	if (ret) {
	    free (*out_creds);
	    *out_creds = NULL;
	} else if (krb5_principal_compare_any_realm(context, (*out_creds)->server, in_creds->server) != TRUE) {
	    krb5_free_creds(context, *out_creds);
	    *out_creds = NULL;
	    ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	}
    }
    krb5_free_creds(context, tgt);
    return ret;
}

/*
get_cred(server)
	creds = cc_get_cred(server)
	if(creds) return creds
	tgt = cc_get_cred(krbtgt/server_realm@any_realm)
	if(tgt)
		return get_cred_tgt(server, tgt)
	if(client_realm == server_realm)
		return NULL
	tgt = get_cred(krbtgt/server_realm@client_realm)
	while(tgt_inst != server_realm)
		tgt = get_cred(krbtgt/server_realm@tgt_inst)
	return get_cred_tgt(server, tgt)
	*/

static krb5_error_code
get_cred_kdc_capath(krb5_context context,
		    krb5_kdc_flags flags,
		    krb5_ccache ccache,
		    krb5_creds *in_creds,
		    krb5_principal impersonate_principal,
		    Ticket *second_ticket,
		    const char *kdc_hostname,
		    krb5_creds **out_creds,
		    krb5_creds ***ret_tgts)
{
    krb5_error_code ret;
    krb5_const_realm client_realm, server_realm, try_realm;

    client_realm = krb5_principal_get_realm(context, in_creds->client);
    server_realm = krb5_principal_get_realm(context, in_creds->server);

    try_realm = client_realm;
    ret = get_cred_kdc_capath_worker(context, flags, ccache, in_creds, try_realm,
                                     impersonate_principal, second_ticket, kdc_hostname,
				     out_creds, ret_tgts);

    if (ret == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN) {
        try_realm = krb5_config_get_string(context, NULL, "capaths",
                                           client_realm, server_realm, NULL);

        if (try_realm != NULL && strcmp(try_realm, client_realm)) {
            ret = get_cred_kdc_capath_worker(context, flags, ccache, in_creds,
                                             try_realm, impersonate_principal,
                                             second_ticket, kdc_hostname,
					     out_creds, ret_tgts);
        }
    }

    return ret;
}

static krb5_error_code
get_cred_kdc_referral(krb5_context context,
		      krb5_kdc_flags flags,
		      krb5_ccache ccache,
		      krb5_creds *in_creds,
		      krb5_principal impersonate_principal,
		      Ticket *second_ticket,			
		      const char *kdc_hostname,
		      krb5_creds **out_creds,
		      krb5_creds ***ret_tgts)
{
    krb5_const_realm client_realm;
    krb5_const_principal server = in_creds->server;
    krb5_error_code ret;
    krb5_creds tgt, referral, ticket;
    int loop = 0;
    int ok_as_delegate = 1;

    if (server->name.name_string.len < 2 && !flags.b.canonicalize) {
	krb5_set_error_message(context, KRB5KDC_ERR_PATH_NOT_ACCEPTED,
			       N_("Name too short to do referals, skipping referals", ""));
	return KRB5KDC_ERR_PATH_NOT_ACCEPTED;
    }
    /* XXX 9268316, make referrals checking saner  */
    if (server->name.name_string.len > 0 && strcmp(server->name.name_string.val[0], "kadmin") == 0) {
	krb5_set_error_message(context, KRB5KDC_ERR_PATH_NOT_ACCEPTED,
			       N_("Name[0] is kadmin, skipping referrals", ""));
	return KRB5KDC_ERR_PATH_NOT_ACCEPTED;
    }

    memset(&tgt, 0, sizeof(tgt));
    memset(&ticket, 0, sizeof(ticket));

    flags.b.canonicalize = 1;

    *out_creds = NULL;

    client_realm = krb5_principal_get_realm(context, in_creds->client);

    /* find tgt for the clients base realm */
    {
	krb5_principal tgtname;

	ret = krb5_make_principal(context, &tgtname,
				  client_realm,
				  KRB5_TGS_NAME,
				  client_realm,
				  NULL);
	if(ret)
	    return ret;

	ret = find_cred(context, ccache, tgtname, *ret_tgts, &tgt);
	krb5_free_principal(context, tgtname);
	if (ret)
	    return ret;
    }

    referral = *in_creds;
    ret = krb5_copy_principal(context, server, &referral.server);
    if (ret) {
	krb5_free_cred_contents(context, &tgt);
	return ret;
    }
    ret = krb5_principal_set_realm(context, referral.server, client_realm);
    if (ret) {
	krb5_free_cred_contents(context, &tgt);
	krb5_free_principal(context, referral.server);
	return ret;
    }

    while (loop++ < 17) {
	krb5_creds **tickets;
	krb5_creds mcreds;
	char *referral_realm;

	/* Use cache if we are not doing impersonation or contrainte deleg */
	if (impersonate_principal == NULL || flags.b.constrained_delegation) {
	    krb5_cc_clear_mcred(&mcreds);
	    mcreds.server = referral.server;
	    ret = krb5_cc_retrieve_cred(context, ccache, 0, &mcreds, &ticket);
	} else
	    ret = EINVAL;

	if (ret) {
	    ret = get_cred_kdc_address(context, ccache, flags, NULL,
				       &referral, &tgt, impersonate_principal,
				       second_ticket, kdc_hostname, &ticket);
	    if (ret)
		goto out;
	}

	/* Did we get the right ticket ? */
	if (krb5_principal_compare_any_realm(context,
					     referral.server,
					     ticket.server))
	    break;

	if (!krb5_principal_is_krbtgt(context, ticket.server)) {
	    krb5_set_error_message(context, KRB5KRB_AP_ERR_NOT_US,
				   N_("Got back an non krbtgt "
				      "ticket referrals", ""));
	    ret = KRB5KRB_AP_ERR_NOT_US;
	    goto out;
	}

	referral_realm = ticket.server->name.name_string.val[1];

	/* check that there are no referrals loops */
	tickets = *ret_tgts;

	krb5_cc_clear_mcred(&mcreds);
	mcreds.server = ticket.server;

	while(tickets && *tickets){
	    if(krb5_compare_creds(context,
				  KRB5_TC_DONT_MATCH_REALM,
				  &mcreds,
				  *tickets))
	    {
		krb5_set_error_message(context, KRB5_GET_IN_TKT_LOOP,
				       N_("Referral from %s "
					  "loops back to realm %s", ""),
				       tgt.server->realm,
				       referral_realm);
		ret = KRB5_GET_IN_TKT_LOOP;
                goto out;
	    }
	    tickets++;
	}

	/*
	 * if either of the chain or the ok_as_delegate was stripped
	 * by the kdc, make sure we strip it too.
	 */

	if (ok_as_delegate == 0 || ticket.flags.b.ok_as_delegate == 0) {
	    ok_as_delegate = 0;
	    ticket.flags.b.ok_as_delegate = 0;
	}

	ret = add_cred(context, &ticket, ret_tgts);
	if (ret)
	    goto out;

	/* try realm in the referral */
	ret = krb5_principal_set_realm(context,
				       referral.server,
				       referral_realm);
	krb5_free_cred_contents(context, &tgt);
	tgt = ticket;
	memset(&ticket, 0, sizeof(ticket));
	if (ret)
	    goto out;
    }

    ret = krb5_copy_creds(context, &ticket, out_creds);

out:
    krb5_free_principal(context, referral.server);
    krb5_free_cred_contents(context, &tgt);
    krb5_free_cred_contents(context, &ticket);
    return ret;
}


/*
 * Glue function between referrals version and old client chasing
 * codebase.
 */

krb5_error_code
_krb5_get_cred_kdc_any(krb5_context context,
		       krb5_kdc_flags flags,
		       krb5_ccache ccache,
		       krb5_creds *in_creds,
		       krb5_principal impersonate_principal,
		       Ticket *second_ticket,
		       krb5_creds **out_creds,
		       krb5_creds ***ret_tgts)
{
    char *kdc_hostname = NULL;
    krb5_error_code ret;
    krb5_deltat offset;
    krb5_data data;

    /*
     * If we are using LKDC, lets pull out the addreses from the
     * ticket and use that.
     */

    ret = krb5_cc_get_config(context, ccache, NULL, "lkdc-hostname", &data);
    if (ret == 0) {
	kdc_hostname = malloc(data.length + 1);
	if (kdc_hostname == NULL) {
	    krb5_set_error_message(context, ENOMEM,
				   N_("malloc: out of memory", ""));
	    return ENOMEM;
	}
	memcpy(kdc_hostname, data.data, data.length);
	kdc_hostname[data.length] = '\0';
    }

    ret = krb5_cc_get_kdc_offset(context, ccache, &offset);
    if (ret == 0) {
	context->kdc_sec_offset = (uint32_t)offset;
	context->kdc_usec_offset = 0;
    }

    ret = get_cred_kdc_referral(context,
				flags,
				ccache,
				in_creds,
				impersonate_principal,
				second_ticket,
				kdc_hostname,
				out_creds,
				ret_tgts);
    if (ret == 0 || flags.b.canonicalize)
	return ret;
    return get_cred_kdc_capath(context,
				flags,
				ccache,
				in_creds,
				impersonate_principal,
				second_ticket,
				kdc_hostname,
				out_creds,
				ret_tgts);
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

/**
 * Get credentials specified by in_cred->server and flags
 *
 * @param context A kerberos 5 context.
 * @param options KRB5_TC_* options
 * @param flags KDC option flags
 * @param ccache credential cache to use.
 * @param in_creds input matching credential.
 * @param out_creds the resulting credential.
 *
 * @return Return an error code or 0.
 *
 * @ingroup krb5_credential
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_credentials_with_flags(krb5_context context,
				krb5_flags options,
				krb5_kdc_flags flags,
				krb5_ccache ccache,
				krb5_creds *in_creds,
				krb5_creds **out_creds)
{
    struct timeval start_time, stop_time;
    krb5_error_code ret;
    krb5_creds **tgts;
    krb5_creds *res_creds;

    gettimeofday(&start_time, NULL);

    if (in_creds->session.keytype) {
	ret = krb5_enctype_valid(context, in_creds->session.keytype);
	if (ret)
	    return ret;
    }

    *out_creds = NULL;
    res_creds = calloc(1, sizeof(*res_creds));
    if (res_creds == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    if (in_creds->session.keytype)
	options |= KRB5_TC_MATCH_KEYTYPE;

    /*
     * If we got a credential, check if credential is expired before
     * returning it. Also check w/o realm since we might have a
     * referrals lookup.
     */
    ret = krb5_cc_retrieve_cred(context,
                                ccache,
                                options,
                                in_creds, res_creds);

    /*
     * If we got a credential, check if credential is expired before
     * returning it, but only if KRB5_GC_EXPIRED_OK is not set.
     */
    if (ret == 0) {
	krb5_timestamp timeret;

	/* If expired ok, don't bother checking */
        if(options & KRB5_GC_EXPIRED_OK) {
            *out_creds = res_creds;
            return 0;
        }

	krb5_timeofday(context, &timeret);
	if(res_creds->times.endtime > timeret) {
	    *out_creds = res_creds;
	    return 0;
	}
	if(options & KRB5_GC_CACHED)
	    krb5_cc_remove_cred(context, ccache, 0, res_creds);

	krb5_free_cred_contents(context, res_creds);

    } else if(ret != KRB5_CC_NOTFOUND) {
        free(res_creds);
        return ret;
    }
    free(res_creds);
    if(options & KRB5_GC_CACHED)
	return not_found(context, in_creds->server, KRB5_CC_NOTFOUND);

    /* if we don't use keytype, lets use negative cache */
    if ((options & KRB5_TC_MATCH_KEYTYPE) == 0 && context->tgs_negative_timeout) {
	krb5_data neg;
	
	ret = krb5_cc_get_config(context, ccache, in_creds->server, "negative-cache", &neg);
	if (ret == 0) {
	    uint32_t t32 = (uint32_t)time(NULL); /* if entry less then 4, its a negativ entry */
	    int32_t rv = KRB5_CC_NOTFOUND; /* if no error code, assume one */
	    krb5_storage *sp;
	    char *estr = NULL;
	    
	    sp = krb5_storage_from_data(&neg);
	    if (sp == NULL) {
		krb5_data_free(&neg);
		return ENOMEM;
	    }
	    
	    ret = krb5_ret_uint32(sp, &t32);
	    if (ret == 0)
		ret = krb5_ret_int32(sp, &rv);
	    if (ret == 0)
		ret = krb5_ret_string(sp, &estr);

	    krb5_storage_free(sp);
	    krb5_data_free(&neg);
	    if (ret) {
		free(estr);
		return ret;
	    }
	    if (abs((uint32_t)time(NULL) - t32) < context->tgs_negative_timeout) { /* negative entry not expired, fail */
		char *str = NULL;
		ret = rv;
		krb5_unparse_name(context, in_creds->server, &str);
		krb5_set_error_message(context, ret,
				       "%s while looking up '%s' (cached result, timeout in %ld sec)",
				       estr ? estr : "<no cached error string>",
				       str ? str : "unknown",
				       context->tgs_negative_timeout  - abs((int)(t32 - time(NULL))));
		free(estr);
		free(str);
		return ret;
	    }
	    free(estr);
	}
    }

    if(options & KRB5_GC_USER_USER)
	flags.b.enc_tkt_in_skey = 1;
    if (flags.b.enc_tkt_in_skey)
	options |= KRB5_GC_NO_STORE;

    tgts = NULL;
    ret = _krb5_get_cred_kdc_any(context, flags, ccache,
				 in_creds, NULL, NULL, out_creds, &tgts);
    if (tgts) {
	store_tgts(context, ccache, tgts);
	free(tgts);
    }
    if(ret == 0 && (options & KRB5_GC_NO_STORE) == 0) {
	krb5_cc_store_cred(context, ccache, *out_creds);

	/*
	 * Store an referrals entry since the server changed from that
	 * expected and if we want to find it again next time, it
	 * better have the right name.
	 *
	 * We only need to compare any realm since the referrals
	 * matching code will do the same for us.
	 */
	if (krb5_principal_compare_any_realm(context, (*out_creds)->server, in_creds->server) == FALSE) {
	    krb5_creds ref = **out_creds;
	    krb5_principal_data refp = *in_creds->server;
	    refp.realm = "";
	    ref.server = &refp;
	    krb5_cc_store_cred(context, ccache, &ref);
	}
    }

    if ((options & KRB5_TC_MATCH_KEYTYPE) == 0 &&
	((ret == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN) ||
	 (ret == KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN)))
    {
	krb5_storage *sp = krb5_storage_emem();
	krb5_data neg;

	if (sp == NULL)
	    goto out;

	(void)krb5_store_uint32(sp, (uint32_t)time(NULL));
	(void)krb5_store_int32(sp, ret);
	if (context->error_code == ret && context->error_string)
	    (void)krb5_store_string(sp, context->error_string);
	
	if (krb5_storage_to_data(sp, &neg) == 0) {
	    (void)krb5_cc_set_config(context, ccache, in_creds->server, "negative-cache", &neg);
	    krb5_data_free(&neg);
	}
	krb5_storage_free(sp);
    }
out:
    gettimeofday(&stop_time, NULL);
    timevalsub(&stop_time, &start_time);
    _krb5_debugx(context, 1, "krb5_get_credentials_with_flags: %s wc: %ld.%06d",
		 in_creds->client->realm,
		 stop_time.tv_sec, stop_time.tv_usec);

    return ret;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_credentials(krb5_context context,
		     krb5_flags options,
		     krb5_ccache ccache,
		     krb5_creds *in_creds,
		     krb5_creds **out_creds)
{
    krb5_kdc_flags flags;
    flags.i = 0;
    return krb5_get_credentials_with_flags(context, options, flags,
					   ccache, in_creds, out_creds);
}

struct krb5_get_creds_opt_data {
    krb5_principal self;
    krb5_flags options;
    krb5_enctype enctype;
    Ticket *ticket;
};


KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_creds_opt_alloc(krb5_context context, krb5_get_creds_opt *opt)
{
    *opt = calloc(1, sizeof(**opt));
    if (*opt == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    return 0;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_get_creds_opt_free(krb5_context context, krb5_get_creds_opt opt)
{
    if (opt->self)
	krb5_free_principal(context, opt->self);
    if (opt->ticket) {
	free_Ticket(opt->ticket);
	free(opt->ticket);
    }
    memset(opt, 0, sizeof(*opt));
    free(opt);
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_get_creds_opt_set_options(krb5_context context,
			       krb5_get_creds_opt opt,
			       krb5_flags options)
{
    opt->options = options;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_get_creds_opt_add_options(krb5_context context,
			       krb5_get_creds_opt opt,
			       krb5_flags options)
{
    opt->options |= options;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_get_creds_opt_set_enctype(krb5_context context,
			       krb5_get_creds_opt opt,
			       krb5_enctype enctype)
{
    opt->enctype = enctype;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_creds_opt_set_impersonate(krb5_context context,
				   krb5_get_creds_opt opt,
				   krb5_const_principal self)
{
    if (opt->self)
	krb5_free_principal(context, opt->self);
    return krb5_copy_principal(context, self, &opt->self);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_creds_opt_set_ticket(krb5_context context,
			      krb5_get_creds_opt opt,
			      const Ticket *ticket)
{
    if (opt->ticket) {
	free_Ticket(opt->ticket);
	free(opt->ticket);
	opt->ticket = NULL;
    }
    if (ticket) {
	krb5_error_code ret;

	opt->ticket = malloc(sizeof(*ticket));
	if (opt->ticket == NULL) {
	    krb5_set_error_message(context, ENOMEM,
				   N_("malloc: out of memory", ""));
	    return ENOMEM;
	}
	ret = copy_Ticket(ticket, opt->ticket);
	if (ret) {
	    free(opt->ticket);
	    opt->ticket = NULL;
	    krb5_set_error_message(context, ret,
				   N_("malloc: out of memory", ""));
	    return ret;
	}
    }
    return 0;
}



KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_creds(krb5_context context,
	       krb5_get_creds_opt opt,
	       krb5_ccache ccache,
	       krb5_const_principal inprinc,
	       krb5_creds **out_creds)
{
    krb5_kdc_flags flags;
    krb5_flags options;
    krb5_creds in_creds;
    krb5_error_code ret;
    krb5_creds **tgts;
    krb5_creds *res_creds;

    if (opt && opt->enctype) {
	ret = krb5_enctype_valid(context, opt->enctype);
	if (ret)
	    return ret;
    }

    memset(&in_creds, 0, sizeof(in_creds));
    in_creds.server = rk_UNCONST(inprinc);

    if (_krb5_have_debug(context, 5)) {
	char *princ;
	ret = krb5_unparse_name(context, inprinc, &princ);
	if (ret == 0) {
	    _krb5_debugx(context, 5, "krb5_get_creds: %s: opt: %d", princ, opt ? opt->options : 0);
	    krb5_xfree(princ);
	}
    }

    ret = krb5_cc_get_principal(context, ccache, &in_creds.client);
    if (ret)
	return ret;

    if (opt)
	options = opt->options;
    else
	options = 0;
    flags.i = 0;

    *out_creds = NULL;
    res_creds = calloc(1, sizeof(*res_creds));
    if (res_creds == NULL) {
	krb5_free_principal(context, in_creds.client);
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    if (opt && opt->enctype) {
	in_creds.session.keytype = opt->enctype;
	options |= KRB5_TC_MATCH_KEYTYPE;
    }

    /*
     * If we got a credential, check if credential is expired before
     * returning it.
     */
    ret = krb5_cc_retrieve_cred(context,
                                ccache,
				options & KRB5_TC_MATCH_KEYTYPE,
                                &in_creds, res_creds);
    /*
     * If we got a credential, check if credential is expired before
     * returning it, but only if KRB5_GC_EXPIRED_OK is not set.
     */
    if (ret == 0) {
	krb5_timestamp timeret;

	/* If expired ok, don't bother checking */
        if(options & KRB5_GC_EXPIRED_OK) {
            *out_creds = res_creds;
	    krb5_free_principal(context, in_creds.client);
            goto out;
        }

	krb5_timeofday(context, &timeret);
	if(res_creds->times.endtime > timeret) {
	    *out_creds = res_creds;
	    krb5_free_principal(context, in_creds.client);
            goto out;
	}
	if(options & KRB5_GC_CACHED)
	    krb5_cc_remove_cred(context, ccache, 0, res_creds);

    } else if(ret != KRB5_CC_NOTFOUND) {
        free(res_creds);
	krb5_free_principal(context, in_creds.client);
	goto out;
    }
    free(res_creds);
    if(options & KRB5_GC_CACHED) {
	krb5_free_principal(context, in_creds.client);
	ret = not_found(context, in_creds.server, KRB5_CC_NOTFOUND);
	goto out;
    }
    if(options & KRB5_GC_USER_USER) {
	flags.b.enc_tkt_in_skey = 1;
	options |= KRB5_GC_NO_STORE;
    }
    if (options & KRB5_GC_FORWARDABLE)
	flags.b.forwardable = 1;
    if (options & KRB5_GC_NO_TRANSIT_CHECK)
	flags.b.disable_transited_check = 1;
    if (options & KRB5_GC_CONSTRAINED_DELEGATION) {
	flags.b.request_anonymous = 1; /* XXX ARGH confusion */
	flags.b.constrained_delegation = 1;
    }
    if (options & KRB5_GC_CANONICALIZE)
	flags.b.canonicalize = 1;

    tgts = NULL;
    ret = _krb5_get_cred_kdc_any(context, flags, ccache,
				 &in_creds, opt ? opt->self : NULL, opt->ticket,
				 out_creds, &tgts);
    krb5_free_principal(context, in_creds.client);
    if (tgts) {
	store_tgts(context, ccache, tgts);
	free(tgts);
    }
    if(ret == 0 && (options & KRB5_GC_NO_STORE) == 0)
	krb5_cc_store_cred(context, ccache, *out_creds);

 out:
    _krb5_debugx(context, 5, "krb5_get_creds: ret = %d", ret);

    return ret;
}

/*
 *
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_renewed_creds(krb5_context context,
		       krb5_creds *creds,
		       krb5_const_principal client,
		       krb5_ccache ccache,
		       const char *in_tkt_service)
{
    krb5_error_code ret;
    krb5_kdc_flags flags;
    krb5_creds in, *template, *out = NULL;

    memset(&in, 0, sizeof(in));
    memset(creds, 0, sizeof(*creds));

    ret = krb5_copy_principal(context, client, &in.client);
    if (ret)
	return ret;

    if (in_tkt_service) {
	ret = krb5_parse_name(context, in_tkt_service, &in.server);
	if (ret) {
	    krb5_free_principal(context, in.client);
	    return ret;
	}
    } else {
	const char *realm = krb5_principal_get_realm(context, client);

	ret = krb5_make_principal(context, &in.server, realm, KRB5_TGS_NAME,
				  realm, NULL);
	if (ret) {
	    krb5_free_principal(context, in.client);
	    return ret;
	}
    }

    flags.i = 0;
    flags.b.renewable = flags.b.renew = 1;

    /*
     * Get template from old credential cache for the same entry, if
     * this failes, no worries.
     */
    ret = krb5_get_credentials(context, KRB5_GC_CACHED, ccache, &in, &template);
    if (ret == 0) {
	flags.b.forwardable = template->flags.b.forwardable;
	flags.b.proxiable = template->flags.b.proxiable;
	krb5_free_creds (context, template);
    }

    ret = krb5_get_kdc_cred(context, ccache, flags, NULL, NULL, &in, &out);
    krb5_free_principal(context, in.client);
    krb5_free_principal(context, in.server);
    if (ret)
	return ret;

    ret = krb5_copy_creds_contents(context, out, creds);
    krb5_free_creds(context, out);

    return ret;
}
