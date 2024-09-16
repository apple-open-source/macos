/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

static krb5_error_code
make_etypelist(krb5_context context, AuthorizationData *if_relevant)
{
    AuthorizationDataElement el;
    EtypeList etypes;
    krb5_error_code ret;
    u_char *buf;
    size_t len = 0;
    size_t buf_size;

    ret = _krb5_init_etype(context, KRB5_PDU_NONE,
			   &etypes.len, &etypes.val,
			   NULL);
    if (ret)
	return ret;

    ASN1_MALLOC_ENCODE(EtypeList, buf, buf_size, &etypes, &len, ret);
    free_EtypeList(&etypes);
    if (ret)
	return ret;
    heim_assert(buf_size == len, "internal error in ASN.1 encoder");

    el.ad_type = KRB5_AUTHDATA_GSS_API_ETYPE_NEGOTIATION;
    el.ad_data.length = len;
    el.ad_data.data = buf;

    ret = add_AuthorizationData(if_relevant, &el);
    free(buf);

    return ret;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_build_authenticator (krb5_context context,
			   krb5_auth_context auth_context,
			   krb5_enctype enctype,
			   krb5_creds *cred,
			   Checksum *cksum,
			   krb5_data *result,
			   krb5_key_usage usage)
{
    AuthorizationData if_relevant;
    Authenticator auth;
    u_char *buf = NULL;
    size_t buf_size;
    size_t len = 0;
    krb5_error_code ret;
    krb5_crypto crypto;

    memset(&auth, 0, sizeof(auth));
    memset(&if_relevant, 0, sizeof(if_relevant));

    auth.authenticator_vno = 5;
    ret = copy_Realm(&cred->client->realm, &auth.crealm);
    if (ret)
	goto fail;

    ret = copy_PrincipalName(&cred->client->name, &auth.cname);
    if (ret)
	goto fail;

    krb5_us_timeofday (context, &auth.ctime, &auth.cusec);

    ret = krb5_auth_con_getlocalsubkey(context, auth_context, &auth.subkey);
    if(ret)
	goto fail;

    if (auth_context->flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE) {
	if(auth_context->local_seqnumber == 0)
	    krb5_generate_seq_number (context,
				      &cred->session,
				      &auth_context->local_seqnumber);
	ALLOC(auth.seq_number, 1);
	if(auth.seq_number == NULL) {
	    ret = ENOMEM;
	    goto fail;
	}
	*auth.seq_number = auth_context->local_seqnumber;
    } else
	auth.seq_number = NULL;

    if (auth_context->auth_data) {
	auth.authorization_data = calloc(1, sizeof(*auth.authorization_data));
	if (auth.authorization_data == NULL) {
	    ret = ENOMEM;
	    goto fail;
	}
	ret = copy_AuthorizationData(auth.authorization_data, auth_context->auth_data);
	if (ret)
	    goto fail;

    } else {
	auth.authorization_data = NULL;
    }

    /*
     * Client wants PFS, add it into the AuthorizationData field.
     */

    if (auth_context->pfs) {
	ret = _krb5_pfs_ap_req(context, auth_context, cred->client, &if_relevant);
	if (ret)
	    goto fail;
    }

    if (cksum) {
	ALLOC(auth.cksum, 1);
	if (auth.cksum == NULL) {
	    ret = ENOMEM;
	    goto fail;
	}
	ret = copy_Checksum(cksum, auth.cksum);
	if (ret)
	    goto fail;

	if (auth.cksum->cksumtype == CKSUMTYPE_GSSAPI) {
	    /*
	     * This is not GSS-API specific, we only enable it for
	     * GSS for now
	     */
	    ret = make_etypelist(context, &if_relevant);
	    if (ret)
		goto fail;
	}
    }

    if (if_relevant.len) {
	AuthorizationDataElement ade;

	ASN1_MALLOC_ENCODE(AD_IF_RELEVANT, buf, buf_size, &if_relevant, &len, ret);
	if (ret)
	    goto fail;
	heim_assert(buf_size == len, "internal error in ASN.1 encoder");

	if (auth.authorization_data == NULL) {
	    ALLOC(auth.authorization_data, 1);
	    if (auth.authorization_data == NULL) {
		ret = krb5_enomem(context);
		goto fail;
	    }
	}

	ade.ad_type = KRB5_AUTHDATA_IF_RELEVANT;
	ade.ad_data.length = len;
	ade.ad_data.data = buf;

	ret = add_AuthorizationData(auth.authorization_data, &ade);
	if (ret)
	    goto fail;
    }


    /* XXX - Copy more to auth_context? */

    auth_context->authenticator->ctime = auth.ctime;
    auth_context->authenticator->cusec = auth.cusec;

    ASN1_MALLOC_ENCODE(Authenticator, buf, buf_size, &auth, &len, ret);
    if (ret)
	goto fail;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_crypto_init(context, &cred->session, enctype, &crypto);
    if (ret)
	goto fail;
    ret = krb5_encrypt (context,
			crypto,
			usage /* KRB5_KU_AP_REQ_AUTH */,
			buf,
			len,
			result);
    krb5_crypto_destroy(context, crypto);

    if (ret)
	goto fail;

 fail:
    free_AuthorizationData(&if_relevant);
    free_Authenticator (&auth);
    free (buf);

    return ret;
}
