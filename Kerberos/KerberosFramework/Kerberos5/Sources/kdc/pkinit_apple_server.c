/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * pkinit_apple_server.c - Server side routines for PKINIT, Mac OS X version
 *
 * Created 21 May 2004 by Doug Mitchell at Apple.
 */

#include "pkinit_server.h"
#include "pkinit_asn1.h"
#include "pkinit_cms.h"
#include <assert.h>

#define     PKINIT_DEBUG    1
#if	    PKINIT_DEBUG
#define     pkiDebug(args...)       printf(args)
#else
#define     pkiDebug(args...)
#endif

/*
 * Parse PA-PK-AS-REQ message. Optionally evaluates the message's certificate chain. 
 * Optionally returns various components. 
 */
krb5_error_code pkinit_as_req_parse(
    const krb5_data	*as_req,
    krb5_timestamp      *ctime,		// optionally RETURNED
    krb5_ui_4		*cusec,		// microseconds, optionally RETURNED
    krb5_ui_4		*nonce,		// optionally RETURNED
    krb5_checksum       *cksum,		// optional, contents mallocd and RETURNED
    pki_cert_sig_status *cert_status,   // optionally RETURNED
    
    /*
     * Describe the ContentInfo : signed and/or encrypted. Both optional.
     */
    krb5_boolean	*is_signed,
    krb5_boolean	*is_encrypted,

    /*
     * Cert fields, all optionally RETURNED.
     *
     * client_cert is the full X.509 leaf cert from the incoming SignedData.
     * all_certs is an array of all of the certs in the incoming SignedData,
     *    in full X.509 form. 
     * kdc_cert and encrypt_cert are IssuerAndSerialNumber fields. 
     */
    krb5_data		*signer_cert,   // content mallocd
    unsigned		*num_all_certs, // sizeof *all_certs
    krb5_data		**all_certs,    // krb5_data's and their content mallocd
    krb5_data		*kdc_cert,      // content mallocd
    krb5_data		*encrypt_cert,  // content mallocd
    
    /*
     * Array of TrustedCAs, optionally RETURNED.
     */
    unsigned		*num_trusted_CAs,   // sizeof *trustedCAs
    krb5_data		**trustedCAs)       // krb5_data's and their content mallocd
{
    krb5_error_code krtn;
    krb5_data signed_auth_pack = {0, 0, NULL};
    krb5_data raw_auth_pack = {0, 0, NULL};
    krb5_data *raw_auth_pack_p = NULL;
    krb5_boolean proceed = FALSE;
    krb5_boolean need_auth_pack = FALSE;
    PKI_ContentType content_type;
    pkinit_cert_db_t cert_db = NULL;
   
    assert(as_req != NULL);
    
    /* 
     * We always have to decode the top-level AS-REQ...
     */
    krtn = pkinit_pa_pk_as_req_decode(as_req, &signed_auth_pack,
	    num_trusted_CAs, trustedCAs,	    // optional
	    kdc_cert, encrypt_cert);		    // optional
    if(krtn) {
	pkiDebug("pkinit_pa_pk_as_req_decode returned %d\n", (int)krtn);
	return krtn;
    }

    /* Do we need info about or from the ContentInto or AuthPack? */
    if((ctime != NULL) || (cusec != NULL) || (nonce != NULL) || (cksum != NULL)) {
	need_auth_pack = TRUE;
	raw_auth_pack_p = &raw_auth_pack;
    }
    if(need_auth_pack || (cert_status != NULL) || (is_signed != NULL) || 
       (is_encrypted != NULL) || (signer_cert != NULL) || (all_certs != NULL)) {
	proceed = TRUE;
    }
    if(!proceed) {
	krtn = 0;
	goto err_out;
    }
    
    /* Parse and possibly verify the ContentInfo */
    krtn = pkinit_get_kdc_cert_db(&cert_db);
    if(krtn) {
	pkiDebug("pa_pk_as_req_parse: error in pkinit_get_kdc_cert_db\n");
	goto err_out;
    }
    krtn = pkinit_parse_content_info(&signed_auth_pack, cert_db,
	is_signed, is_encrypted,
	raw_auth_pack_p, &content_type, signer_cert, cert_status, 
	num_all_certs, all_certs);
    if(krtn) {
	pkiDebug("pkinit_parse_content_info returned %d\n", (int)krtn);
	goto err_out;
    }

    /* optionally parse contents of authPack */
    if(need_auth_pack) {
	krtn = pkinit_auth_pack_decode(&raw_auth_pack, ctime, cusec, nonce, cksum);
	if(krtn) {
	    pkiDebug("pkinit_auth_pack_decode returned %\n", (int)krtn);
	    goto err_out;
	}
    }

err_out:
    /* free temp mallocd data that we didn't pass back to caller */
    if(signed_auth_pack.data) {
	free(signed_auth_pack.data);
    }
    if(raw_auth_pack.data) {
	free(raw_auth_pack.data);
    }
    if(cert_db) {
	pkinit_release_cert_db(cert_db);
    }
    return krtn;
}

/*
 * Create a PA-PK-AS-REP message, public key (no Diffie Hellman) version.
 *
 * PA-PK-AS-REP is based on ReplyKeyPack like so:
 *
 * PA-PK-AS-REP ::= EnvelopedData(SignedData(ReplyKeyPack))
 */
krb5_error_code pkinit_as_rep_create(
    const krb5_keyblock     *key_block,
    krb5_ui_4		    nonce,
    pkinit_signing_cert_t   signer_cert,	    // server's cert
    krb5_boolean	    include_server_cert,    // include signer_cert in SignerInfo
    const krb5_data	    *recipient_cert,	    // client's cert
    krb5_data		    *as_rep)		    // mallocd and RETURNED
{
    krb5_data reply_key_pack = {0, 0, NULL};
    krb5_error_code krtn;
    krb5_data signed_data = {0, 0, NULL};
    krb5_data enc_key_pack = {0, 0, NULL};
    
    /* innermost content = ReplyKeyPack */
    krtn = pkinit_reply_key_pack_encode(key_block, nonce, &reply_key_pack);
    if(krtn) {
	return krtn;
    }
    
    /* 
     * Put that in a SignedData
     * -- our cert in SignerInfo optional
     * -- EncapsulatedData.ContentType = pkrkeydata
     */
    krtn = pkinit_create_signed_data(&reply_key_pack, signer_cert, include_server_cert,
	ECT_PkReplyKeyKata, &signed_data);
    if(krtn) {
	goto err_out;
    }
    
    /* 
     * Put that in an EnvelopedData
     * -- encrypted with client's cert
     * -- EncryptedContentInfo.ContentType = id-signedData
     */
    krtn = pkinit_create_envel_data(&signed_data, recipient_cert, ECT_SignedData,
	&enc_key_pack);
    if(krtn) {
	goto err_out;
    }
    
    /*
     * Finally, wrap that inside of PA-PK-AS-REP
     */
    krtn = pkinit_pa_pk_as_rep_encode(NULL, &enc_key_pack, as_rep);
    
err_out:
    if(reply_key_pack.data) {
	free(reply_key_pack.data);
    }
    if(signed_data.data) {
	free(signed_data.data);
    }
    if(enc_key_pack.data) {
	free(enc_key_pack.data);
    }
    return krtn;
}
