/*
 * Copyright (c) 2014 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2014 Apple Inc. All rights reserved.
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

#include <corecrypto/ccec.h>
#include <corecrypto/ccec25519.h>

#include <CommonCrypto/CommonRandomSPI.h>

#include <pkinit_asn1.h>


struct _krb5_pfs_data {
    ccec25519secretkey x25519priv;
    ccec25519pubkey x25519pub;
    ccec_full_ctx_t p256;
    krb5_keyblock keyblock;
    AlgorithmIdentifier ai;

    int config;

    krb5_principal client;
    KRB5_PFS_GROUP group;
};

#define PFS_X25519	1
#define PFS_P256	2

/*
 *
 */

static int
eval_string(krb5_context context, int value, const char *s)
{
    int negative = 0;
    int val = 0;

    if (s[0] == '-') {
	negative = 1;
	s++;
    }

    if (strcasecmp(s, "ALL") == 0) {
	val = PFS_X25519 | PFS_P256;
    } else if (strcasecmp(s, "dh25519") == 0) {
	val = PFS_X25519;
    } else if (strcasecmp(s, "nist-p256") == 0) {
	val = PFS_P256;
    } else if (strcasecmp(s, "p256") == 0) {
	val = PFS_P256;
    } else if (strcasecmp(s, "all-nist") == 0) {
	val = PFS_P256;
    } else {
	_krb5_debugx(context, 10, "unsupported dh curve(s): %s", s);
	return value;
    }
    
    if (negative)
	value &= ~val;
    else
	value |= val;

    return value;
}

static int
_krb5_pfs_parse_pfs_config(krb5_context context, const char *name, const char *defval)
{
    char **strs, **s;
    int val = 0;

    strs = krb5_config_get_strings(context, NULL, "libdefaults", name, NULL);
    if (strs) {
	for(s = strs; s && *s; s++) {
	    val = eval_string(context, val, *s);
	}
    } else {
	val = eval_string(context, 0, defval);
    }
    krb5_config_free_strings(strs);

    return val;
}

/*
 *
 */

krb5_error_code
_krb5_auth_con_setup_pfs(krb5_context context,
			 krb5_auth_context auth_context,
			 krb5_enctype enctype)
{
    ccec_const_cp_t cp = ccec_cp_256();
    struct ccrng_state *rng = ccDRBGGetRngState();
    struct _krb5_pfs_data *pfs = NULL;

    _krb5_debugx(context, 20, "Setting up PFS for auth context");

    pfs = calloc(1, sizeof(*pfs));
    if (pfs == NULL)
	return krb5_enomem(context);

    pfs->config = _krb5_pfs_parse_pfs_config(context, "ap-req-dh-groups", "ALL");
    if (pfs->config == 0) {
	_krb5_debugx(context, 10, "No PFS configuration");
	free(pfs);
	return 0;
    }

    switch (enctype) {
    case KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96:
    case KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96:
    case KRB5_ENCTYPE_DES3_CBC_SHA1:
    case KRB5_ENCTYPE_ARCFOUR_HMAC_MD5:
	pfs->ai.algorithm = asn1_oid_id_pkinit_kdf_ah_sha512;
	pfs->ai.parameters = NULL;
	break;
    default:
	free(pfs);
	return 0;
    }

    pfs->p256 = (ccec_full_ctx_t)calloc(1, ccec_full_ctx_size(ccec_ccn_size(cp)));
    if (pfs->p256 == NULL) {
	free(pfs);
	return krb5_enomem(context);
    }
    
    cccurve25519_make_key_pair(rng, pfs->x25519pub, pfs->x25519priv);

    if (ccec_generate_key_fips(cp, rng, pfs->p256)) {
	krb5_free_keyblock_contents(context, &pfs->keyblock);
	free(pfs->p256);
	free(pfs);
	return krb5_enomem(context);
    }

    auth_context->pfs = pfs;

    return 0;
}

/*
 *
 */

void
_krb5_auth_con_free_pfs(krb5_context context, krb5_auth_context auth_context)
{
    if (auth_context->pfs) {
	free(auth_context->pfs->p256);
	krb5_free_keyblock_contents(context, &auth_context->pfs->keyblock);
	krb5_free_principal(context, auth_context->pfs->client);

	memset(auth_context->pfs, 0, sizeof(*auth_context->pfs));
	free(auth_context->pfs);
	auth_context->pfs = NULL;
    }
}

/*
 *
 */

static krb5_error_code
pfs_make_share_secret(krb5_context context,
		      krb5_auth_context auth_context,
		      krb5_enctype enctype,
		      KRB5_PFS_SELECTION *peer)
{
    struct _krb5_pfs_data *pfs = auth_context->pfs;
    krb5_data shared_secret;
    krb5_error_code ret;

    heim_assert(pfs, "no PFS requestd");

    krb5_data_zero(&shared_secret);
	
    if (peer->group == KRB5_PFS_X25519 && (pfs->config & PFS_X25519)) {
	if (peer->public_key.length != sizeof(ccec25519pubkey)) {
	    krb5_set_error_message(context, HEIM_PFS_GROUP_INVALID,
				   "public key of wrong length");
	    return HEIM_PFS_GROUP_INVALID;
	}

	ret = krb5_data_alloc(&shared_secret, sizeof(ccec25519key));
	if (ret)
	    goto out;

	cccurve25519(shared_secret.data,
		     pfs->x25519priv,
		     peer->public_key.data);

    } else if (peer->group == KRB5_PFS_NIST_P256 && (pfs->config & PFS_P256)) {
	ccec_const_cp_t cp = ccec_cp_256();
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla"
#endif
	ccec_pub_ctx_decl_cp(cp, pubkey);
#ifdef __clang__
#pragma clang diagnostic pop
#endif

	if (ccec_import_pub(cp, peer->public_key.length, peer->public_key.data, pubkey)) {
	    krb5_set_error_message(context, HEIM_PFS_GROUP_INVALID,
				   "failed to import public key");
	    return HEIM_PFS_GROUP_INVALID;
	}

	ret = krb5_data_alloc(&shared_secret, ccec_ccn_size(cp));
	if (ret)
	    goto out;

	if (ccec_compute_key(pfs->p256, pubkey, &shared_secret.length, shared_secret.data)) {
	    krb5_set_error_message(context, HEIM_PFS_GROUP_INVALID,
				   "Failed to complete share key");
	    return HEIM_PFS_GROUP_INVALID;
	}
	
    } else {
	krb5_set_error_message(context, HEIM_PFS_GROUP_INVALID,
			       "Group %d not accepted",
			       (int)peer->group);
	return HEIM_PFS_GROUP_INVALID;
    }

    ret = _krb5_pk_kdf(context,
		       &pfs->ai,
		       shared_secret.data,
		       shared_secret.length,
		       pfs->client,
		       NULL,
		       enctype,
		       NULL,
		       NULL,
		       NULL,
		       &pfs->keyblock);
    if (ret)
	goto out;

    _krb5_debug_keyblock(context, 20, "PFS shared keyblock", &pfs->keyblock);

    pfs->group = peer->group;

 out:
    memset(shared_secret.data, 0, shared_secret.length);
    krb5_data_free(&shared_secret);

    return ret;
}

/*
 *
 */

krb5_error_code
_krb5_pfs_update_key(krb5_context context,
		     krb5_auth_context auth_context,
		     const char *direction,
		     krb5_keyblock *subkey)
{
    struct _krb5_pfs_data *pfs = auth_context->pfs;
    krb5_keyblock newkey;
    krb5_error_code ret;

    memset(&newkey, 0, sizeof(newkey));

    heim_assert(pfs, "no PFS requestd");
    heim_assert(pfs->keyblock.keytype, "shared secret completed");
    heim_assert(pfs->group != KRB5_PFS_INVALID, "no pfs group selected");

    _krb5_debugx(context, 10, "krb5_pfs: updating to PFS key for direction %s", direction);

    ret = _krb5_fast_cf2(context, subkey,
			 "AP PFS shared key",
			 &pfs->keyblock,
			 direction,
			 &newkey,
			 NULL);
    /*
     * If enctype doesn't support PRF, ignore the PFS variable, the
     * peer should not have proposed it in the first place when the
     * poor supported enctype was selected.
     */
    if (ret == HEIM_PFS_ENCTYPE_PRF_NOT_SUPPORTED)
	return 0;
    else if (ret)
	return ret;

    krb5_free_keyblock_contents(context, subkey);
    *subkey = newkey;

    _krb5_debug_keyblock(context, 20, direction, subkey);

    return 0;
}

static krb5_error_code
fill_x25519_proposal(KRB5_PFS_SELECTION *sel, 
		     ccec25519pubkey pubkey)
{
    sel->group = KRB5_PFS_X25519;
    return krb5_data_copy(&sel->public_key, pubkey, sizeof(ccec25519pubkey));
}

static krb5_error_code
add_x25519_proposal(krb5_context context,
		    KRB5_PFS_SELECTIONS *sels, 
		    ccec25519pubkey pubkey)
{
    KRB5_PFS_SELECTION sel;
    krb5_error_code ret;

    ret = fill_x25519_proposal(&sel, pubkey);
    if (ret)
	goto out;

    ret = add_KRB5_PFS_SELECTIONS(sels, &sel);
 out:
    free_KRB5_PFS_SELECTION(&sel);
    return ret;
}

static krb5_error_code
fill_nist_proposal(krb5_context context,
		   KRB5_PFS_SELECTION *sel, 
		   KRB5_PFS_GROUP group,
		   ccec_full_ctx_t fullkey)
{
    krb5_error_code ret;

    sel->group = group;
    sel->public_key.length = ccec_export_pub_size(ccec_ctx_pub(fullkey));
    sel->public_key.data = malloc(sel->public_key.length);
    if (sel->public_key.data == NULL) {
	ret = krb5_enomem(context);
	goto out;
    }

    ccec_export_pub(ccec_ctx_pub(fullkey), sel->public_key.data);
    ret = 0;

 out:
    return ret;
}

static krb5_error_code
add_nist_proposal(krb5_context context,
		  KRB5_PFS_SELECTIONS *sels, 
		  KRB5_PFS_GROUP group,
		  ccec_full_ctx_t fullkey)
{
    KRB5_PFS_SELECTION sel;
    krb5_error_code ret;

    ret = fill_nist_proposal(context, &sel, group, fullkey);
    if (ret)
	goto out;

    ret = add_KRB5_PFS_SELECTIONS(sels, &sel);
 out:
    free_KRB5_PFS_SELECTION(&sel);

    return ret;
}

static krb5_boolean
enctype_pfs_supported(krb5_context context,
		      krb5_auth_context auth_context)
{
    krb5_error_code ret;
    size_t size;

    ret = krb5_crypto_prf_length(context,
				 auth_context->keyblock->keytype,
				 &size);
    if (ret == HEIM_PFS_ENCTYPE_PRF_NOT_SUPPORTED) {
	_krb5_debugx(context, 10, "Enctype %d doesn't support PFS", 
		     auth_context->keyblock->keytype);
	return FALSE;
    }
    if (ret || size == 0)
	return FALSE;

    return TRUE;
}


/*
 *
 *
 */

krb5_error_code
_krb5_pfs_ap_req(krb5_context context,
		 krb5_auth_context auth_context,
		 krb5_const_principal client,
		 AuthorizationData *output)
{
    struct _krb5_pfs_data *pfs = auth_context->pfs;
    AuthorizationDataElement ade;
    krb5_crypto crypto = NULL;
    KRB5_PFS_PROPOSE pp;
    krb5_error_code ret;
    size_t size = 0;
    krb5_data data;
    
    memset(&ade, 0, sizeof(ade));
    memset(&pp, 0, sizeof(pp));
    krb5_data_zero(&data);

    if (!enctype_pfs_supported(context, auth_context))
	return 0;

    if (pfs->config & PFS_X25519) {
	ret = add_x25519_proposal(context, &pp.selections, pfs->x25519pub);
	if (ret)
	    goto out;
    }

    if (pfs->config & PFS_P256) {
	ret = add_nist_proposal(context, &pp.selections, KRB5_PFS_NIST_P256, pfs->p256);
	if (ret)
	    goto out;
    }

    ret = krb5_copy_principal(context, client, &pfs->client);
    if (ret) {
	_krb5_auth_con_free_pfs(context, auth_context);
	free_KRB5_PFS_PROPOSE(&pp);
	return ret;
    }

    /*
     * Create signture
     */

    ret = krb5_crypto_init(context, auth_context->keyblock, 0, &crypto);
    if (ret) 
	goto out;

    ASN1_MALLOC_ENCODE(KRB5_PFS_PROPOSE, data.data, data.length, &pp, &size, ret);
    if (ret)
	goto out;
    heim_assert(size == data.length, "internal asn1 error");

    pp.checksum = calloc(1, sizeof(*pp.checksum));
    if (pp.checksum == NULL) {
	ret = krb5_enomem(context);
	goto out;
    }

    ret = krb5_create_checksum(context, crypto, KRB5_KU_H5L_PFS_AP_CHECKSUM_CLIENT, 0,
			       data.data, data.length, pp.checksum);
    if (ret)
	goto out;

    /*
     * Encode and add
     */

    ade.ad_type = KRB5_AUTHDATA_PFS;

    ASN1_MALLOC_ENCODE(KRB5_PFS_PROPOSE, ade.ad_data.data, ade.ad_data.length, &pp, &size, ret);
    if (ret)
	goto out;
    if (ade.ad_data.length != size)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = add_AuthorizationData(output, &ade);
    if (ret)
	goto out;

 out:
    if (crypto)
	krb5_crypto_destroy(context, crypto);
    free_KRB5_PFS_PROPOSE(&pp);
    free_AuthorizationDataElement(&ade);
    free(data.data);

    return ret;
}

/*
 *
 */

static krb5_error_code
_validate_rd_req_checksum(krb5_context context, krb5_keyblock *keyblock, KRB5_PFS_PROPOSE *pp)
{
    krb5_error_code ret;
    krb5_crypto crypto;
    Checksum *checksum;
    krb5_data data;
    size_t size = 0;

    checksum = pp->checksum;
    if (checksum == NULL) {
	krb5_set_error_message(context, HEIM_PFS_CHECKSUM_WRONG, "peer sent no checksum");
	return HEIM_PFS_CHECKSUM_WRONG;
    }

    pp->checksum = NULL;

    ASN1_MALLOC_ENCODE(KRB5_PFS_PROPOSE, data.data, data.length, pp, &size, ret);
    pp->checksum = checksum;
    if (ret) {
	return ret;
    }
    heim_assert(data.length == size, "internal asn1 encode error");

    ret = krb5_crypto_init(context, keyblock, 0, &crypto);
    if (ret) {
	free(data.data);
	return ret;
    }

    if (!krb5_checksum_is_keyed(context, checksum->cksumtype)) {
	free(data.data);
	ret = HEIM_PFS_CHECKSUM_WRONG;
	krb5_set_error_message(context, ret, "checksum not keyed");
	return ret;
    }

    ret = krb5_verify_checksum(context, crypto, KRB5_KU_H5L_PFS_AP_CHECKSUM_CLIENT,
			       data.data, data.length, checksum);
    krb5_crypto_destroy(context, crypto);
    free(data.data);

    return ret;
}


/*
 *
 */

krb5_error_code
_krb5_pfs_rd_req(krb5_context context,
		 krb5_auth_context auth_context)
{
    KRB5_PFS_SELECTION *selected = NULL;
    AuthorizationData *ad = NULL;
    KRB5_PFS_PROPOSE pp;
    krb5_error_code ret;
    krb5_data data;
    size_t size = 0;
    unsigned n;
    
    memset(&pp, 0, sizeof(pp));
    krb5_data_zero(&data);

    if (auth_context->authenticator == NULL)
	return 0;

    ad = auth_context->authenticator->authorization_data;
    if (ad == NULL)
	return 0;

    heim_assert(auth_context->keyblock, "pfs: don't have keyblock");

    if (!enctype_pfs_supported(context, auth_context))
	return 0;

    ret = _krb5_get_ad(context, ad, NULL, KRB5_AUTHDATA_PFS, &data);
    if (ret)
	return 0;

    ret = decode_KRB5_PFS_PROPOSE(data.data, data.length, &pp, &size);
    krb5_data_free(&data);
    if (ret) 
	goto fail;

    ret = _validate_rd_req_checksum(context, auth_context->keyblock, &pp);
    if (ret)
	goto fail;
    
    ret = _krb5_auth_con_setup_pfs(context, auth_context, auth_context->keyblock->keytype);
    if (ret)
	goto fail;

    for (n = 0; n < pp.selections.len && selected == NULL; n++) {
	if (pp.selections.val[n].group == KRB5_PFS_X25519 && (auth_context->pfs->config & PFS_X25519))
	    selected = &pp.selections.val[n];
	else if (pp.selections.val[n].group == KRB5_PFS_NIST_P256  && (auth_context->pfs->config & PFS_P256))
	    selected = &pp.selections.val[n];
    }

    if (selected == NULL) {
	krb5_set_error_message(context, HEIM_PFS_GROUP_INVALID, "No acceptable PFS group sent");
	ret = HEIM_PFS_GROUP_INVALID;
	goto fail;
    }

    ret = _krb5_principalname2krb5_principal(context, &auth_context->pfs->client,
					     auth_context->authenticator->cname,
					     auth_context->authenticator->crealm);
    if (ret)
	goto fail;
    
    ret = pfs_make_share_secret(context, auth_context,
				auth_context->keyblock->keytype,
				selected);
    if (ret)
	goto fail;

    free_KRB5_PFS_PROPOSE(&pp);

    _krb5_debugx(context, 10, "PFS server made selected");

    return 0;

 fail:
    free_KRB5_PFS_PROPOSE(&pp);
    _krb5_auth_con_free_pfs(context, auth_context);
    return ret;
}

/*
 *
 */

krb5_error_code
_krb5_pfs_mk_rep(krb5_context context,
		 krb5_auth_context auth_context,
		 AP_REP *rep)
{
    struct _krb5_pfs_data *pfs = auth_context->pfs;
    krb5_error_code ret;
    krb5_crypto crypto = NULL;
    krb5_data data;
    size_t size = 0;

    heim_assert(pfs, "no pfs");
    heim_assert(pfs->group != KRB5_PFS_INVALID, "no pfs group selected");

    krb5_data_zero(&data);

    rep->pfs = calloc(1, sizeof(*rep->pfs));
    if (rep->pfs == NULL)
	return krb5_enomem(context);

    if (pfs->group == KRB5_PFS_X25519) {
	ret = fill_x25519_proposal(&rep->pfs->selection, pfs->x25519pub);
	if (ret)
	    goto out;
    } else if (pfs->group == KRB5_PFS_NIST_P256) {
	ret = fill_nist_proposal(context, &rep->pfs->selection, KRB5_PFS_NIST_P256, pfs->p256);
	if (ret)
	    goto out;
    } else {
	heim_assert(0, "Invalid PFS group");
    }

    /*
     *
     */

    ret = krb5_crypto_init(context, auth_context->keyblock, 0, &crypto);
    if (ret) 
	goto out;

    ASN1_MALLOC_ENCODE(KRB5_PFS_ACCEPT, data.data, data.length, rep->pfs, &size, ret);
    if (ret)
	goto out;
    heim_assert(size == data.length, "internal asn1 error");

    rep->pfs->checksum = calloc(1, sizeof(*rep->pfs->checksum));
    if (rep->pfs->checksum == NULL) {
	ret = krb5_enomem(context);
	goto out;
    }

    ret = krb5_create_checksum(context, crypto, KRB5_KU_H5L_PFS_AP_CHECKSUM_SERVER, 0,
			       data.data, data.length, rep->pfs->checksum);
    if (ret)
	goto out;

    /*
     *
     */

    _krb5_debugx(context, 20, "PFS deriving new keys on server");

    ret = _krb5_pfs_update_key(context, auth_context, "session key", auth_context->keyblock);
    if (ret)
	goto out;

    if (auth_context->local_subkey) {
	ret = _krb5_pfs_update_key(context, auth_context, "server key", auth_context->local_subkey);
	if (ret)
	    goto out;
    }

    if (auth_context->remote_subkey) {
	ret = _krb5_pfs_update_key(context, auth_context, "client key", auth_context->remote_subkey);
	if (ret)
	    goto out;
    }

    auth_context->flags |= KRB5_AUTH_CONTEXT_USED_PFS;

 out:
    _krb5_auth_con_free_pfs(context, auth_context);
    krb5_crypto_destroy(context, crypto);
    if (data.data)
	free(data.data);
    if (ret) {
	if (rep->pfs) {
	    free_KRB5_PFS_ACCEPT(rep->pfs);
	    free(rep->pfs);
	    rep->pfs = NULL;
	}
    }

    return ret;
}

/*
 *
 */

static krb5_error_code
_validate_rd_rep_checksum(krb5_context context, krb5_keyblock *keyblock, KRB5_PFS_ACCEPT *aa)
{
    krb5_error_code ret;
    krb5_crypto crypto;
    Checksum *checksum;
    krb5_data data;
    size_t size = 0;

    checksum = aa->checksum;
    if (checksum == NULL) {
	krb5_set_error_message(context, HEIM_PFS_CHECKSUM_WRONG, "peer sent no checksum");
	return HEIM_PFS_CHECKSUM_WRONG;
    }
    
    aa->checksum = NULL;

    ASN1_MALLOC_ENCODE(KRB5_PFS_ACCEPT, data.data, data.length, aa, &size, ret);
    aa->checksum = checksum;
    if (ret) {
	return ret;
    }
    heim_assert(data.length == size, "internal asn1 encode error");

    ret = krb5_crypto_init(context, keyblock, 0, &crypto);
    if (ret) {
	free(data.data);
	return ret;
    }

    if (!krb5_checksum_is_keyed(context, checksum->cksumtype)) {
	free(data.data);
	ret = HEIM_PFS_CHECKSUM_WRONG;
	krb5_set_error_message(context, ret, "checksum not keyed");
	return ret;
    }

    ret = krb5_verify_checksum(context, crypto, KRB5_KU_H5L_PFS_AP_CHECKSUM_SERVER,
			       data.data, data.length, checksum);
    krb5_crypto_destroy(context, crypto);
    free(data.data);

    return ret;
}

/*
 *
 */

krb5_error_code
_krb5_pfs_rd_rep(krb5_context context, krb5_auth_context auth_context, AP_REP *ap_rep)
{
    krb5_error_code ret;

    heim_assert(auth_context->pfs, "no pfs");
    heim_assert(ap_rep->pfs, "no pfs from server");

    ret = _validate_rd_rep_checksum(context, auth_context->keyblock, ap_rep->pfs);
    if (ret)
	goto out;

    ret = pfs_make_share_secret(context, auth_context,
				auth_context->keyblock->keytype,
				&ap_rep->pfs->selection);
    if (ret)
	goto out;

    _krb5_debugx(context, 10, "PFS client made secret");
    _krb5_debugx(context, 20, "PFS deriving new keys on client");

    ret = _krb5_pfs_update_key(context, auth_context, "session key", auth_context->keyblock);
    if (ret)
	goto out;

    if (auth_context->local_subkey) {
	ret = _krb5_pfs_update_key(context, auth_context, "client key", auth_context->local_subkey);
	if (ret)
	    goto out;
    }

    heim_assert(auth_context->pfs->group != KRB5_PFS_INVALID, "no pfs group selected");
    auth_context->flags |= KRB5_AUTH_CONTEXT_USED_PFS;
 out:
    if (ret) {
	_krb5_auth_con_free_pfs(context, auth_context);
    }

    return ret;
}
