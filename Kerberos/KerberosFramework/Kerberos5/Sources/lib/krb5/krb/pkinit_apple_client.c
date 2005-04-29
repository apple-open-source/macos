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
 * pkinit_apple_client.c - Client side routines for PKINIT, Mac OS X version
 *
 * Created 20 May 2004 by Doug Mitchell at Apple.
 */

#include "pkinit_client.h"
#include "pkinit_asn1.h"
#include "pkinit_apple_utils.h"
#include "pkinit_cms.h"
#include <assert.h>
#include <sys/errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * Create a PA-PK-AS-REQ message.
 */
krb5_error_code pkinit_as_req_create(
    krb5_timestamp	    ctime,      
    krb5_ui_4		    cusec,	    // microseconds
    krb5_ui_4		    nonce,
    const krb5_checksum     *cksum,
    pkinit_signing_cert_t   client_cert,    // required
    const krb5_data	    *kdc_cert,      // optional
    krb5_data		    *as_req)	    // mallocd and RETURNED
{
    krb5_data auth_pack = {0, 0, NULL};
    krb5_error_code krtn;
    krb5_data content_info = {0, 0, NULL};
    krb5_data issuerSerial = {0, 0, NULL};
    krb5_data *isp = NULL;
    
    /* encode the core authPack */
    krtn = pkinit_auth_pack_encode(ctime, cusec, nonce, cksum, &auth_pack);
    if(krtn) {
	return krtn;
    }

    /* package the AuthPack up in a SignedData inside a ContentInfo */
    krtn = pkinit_create_signed_data(&auth_pack, client_cert, TRUE, ECT_PkAuthData, 
	&content_info);
    if(krtn) {
	goto errOut;
    }
    
    /* If we have a KDC cert, extract its issuer/serial */
    if(kdc_cert) {
	krtn = pkiGetIssuerAndSerial(kdc_cert, &issuerSerial);
	if(krtn) {
	    goto errOut;
	}
	isp = &issuerSerial;
    }
    
    /* cook up PA-PK-AS-REQ */
    krtn = pkinit_pa_pk_as_req_encode(&content_info, 
	0, NULL,	    // trusted CAs - we don't use them
	isp,		    // optional
	NULL,		    // encryption_cert - we don't use
	as_req);
    
errOut:
    if(auth_pack.data) {
	free(auth_pack.data);
    }
    if(content_info.data) {
	free(content_info.data);
    }
    return krtn;
}

/*
 * Parse PA-PK-AS-REP message. Optionally evaluates the message's certificate chain. 
 * Optionally returns various components. 
 */
krb5_error_code pkinit_as_rep_parse(
    const krb5_data	    *as_rep,
    pkinit_signing_cert_t   client_cert,    // required
    krb5_keyblock	    *key_block,     // RETURNED
    krb5_ui_4		    *nonce,		// RETURNED
    pki_cert_sig_status     *cert_status,   // RETURNED
    
    /*
     * Describe the ContentInfos : signed and/or encrypted. 
     * Both should be true for a valid PA-PK-AS-REP.
     */
    krb5_boolean	    *is_signed,
    krb5_boolean	    *is_encrypted,

    /*
     * Cert fields, all optionally RETURNED.
     *
     * signer_cert is the full X.509 leaf cert from the incoming SignedData.
     * all_certs is an array of all of the certs in the incoming SignedData,
     *    in full X.509 form. 
     */
    krb5_data		    *signer_cert,   // content mallocd
    unsigned		    *num_all_certs, // sizeof *all_certs
    krb5_data		    **all_certs)    // krb5_data's and their content mallocd
{
    krb5_data reply_key_pack = {0, 0, NULL};
    krb5_error_code krtn;
    krb5_data signed_data = {0, 0, NULL};
    krb5_data enc_key_pack = {0, 0, NULL};
    krb5_data dh_signed_data = {0, 0, NULL};
    PKI_ContentType content_type;
    krb5_boolean local_signed = FALSE;
    krb5_boolean local_encrypted = FALSE;
    pkinit_cert_db_t cert_db = NULL;
    
    assert((as_rep != NULL) && (is_signed != NULL) && (is_encrypted != NULL) &&
	   (nonce != NULL) && (key_block != NULL) && (cert_status != NULL));
    
    *is_signed = FALSE;
    *is_encrypted = FALSE;
    
    /* 
     * Decode the top-level PA-PK-AS-REP
     */
    krtn = pkinit_pa_pk_as_rep_decode(as_rep, &dh_signed_data, &enc_key_pack);
    if(krtn) {
	pkiCssmErr("pkinit_pa_pk_as_rep_decode", krtn);
	return krtn;
    }
    if(dh_signed_data.data) {
	/* not for this implementation... */
	pkiDebug("pkinit_as_rep_parse: unexpected dh_signed_data\n");
	krtn = ASN1_BAD_FORMAT;
	goto err_out;
    }
    if(enc_key_pack.data == NULL) {
	/* REQUIRED for this imnplementation... */
	pkiDebug("pkinit_as_rep_parse: no enc_key_pack\n");
	krtn = ASN1_BAD_FORMAT;
	goto err_out;
    }
   
    /*
     * enc_key_pack is an EnvelopedData, encrypted with our cert (which 
     * pkinit_parse_content_info() finds implicitly).
     */
    krtn = pkinit_parse_content_info(&enc_key_pack, NULL, 
	&local_signed, &local_encrypted,
	&signed_data, &content_type,
	/* remaining fields for for SignedData only, haven't gotten there yet */
	NULL, NULL, NULL, NULL);
    if(krtn) {
	pkiDebug("pkinit_as_rep_parse: error decoding EnvelopedData\n");
	goto err_out;
    }
    if(local_encrypted) {
	*is_encrypted = TRUE;
    }
    if(local_signed) {
	pkiDebug("**WARNING: pkinit_as_rep_parse: first CMS parse yielded signed data!\n");
	/* proceed, though we're probably hosed */
    }
    
    /* 
     * The Content of that EnvelopedData is a SignedData...
     */
    krtn = pkinit_get_client_cert_db(NULL, client_cert, &cert_db);
    if(krtn) {
	pkiDebug("pkinit_as_rep_parse: error in pkinit_get_client_cert_db\n");
	goto err_out;
    }
    krtn = pkinit_parse_content_info(&signed_data, cert_db, 
	&local_signed, &local_encrypted,
	&reply_key_pack, &content_type,
	/* now pass in the caller's requested fields */
	signer_cert, cert_status, num_all_certs, all_certs);
    if(krtn) {
	pkiDebug("pa_pk_as_rep_parse: error decoding SignedData\n");
	goto err_out;
    }
    if(local_encrypted) {
	pkiDebug("**WARNING: pkinit_as_rep_parse: 2nd CMS parse yielded encrypted!\n");
    }
    if(local_signed) {
	*is_signed = TRUE;
    }
    
    /* FIXME: verify content_type when our CMS module can handle custom OIDs there */
     
    /* 
     * Finally, decode that SignedData's Content as the ReplyKeyPack which contains
     * the actual key and nonce
     */
    krtn = pkinit_reply_key_pack_decode(&reply_key_pack, key_block, nonce);
    if(krtn) {
	pkiDebug("pkinit_as_rep_parse: error decoding ReplyKeyPack\n");
    }
    
err_out:
    /* free temp mallocd data that we didn't pass back to caller */
    if(reply_key_pack.data) {
	free(reply_key_pack.data);
    }
    if(signed_data.data) {
	free(signed_data.data);
    }
    if(enc_key_pack.data) {
	free(enc_key_pack.data);
    }
    if(dh_signed_data.data) {
	free(dh_signed_data.data);
    }
    if(cert_db) {
	pkinit_release_cert_db(cert_db);
    }
    return krtn;
}

/*
 * Handy place to have a platform-dependent random number generator, e.g., /dev/random. 
 */
krb5_error_code pkinit_rand(
    void *dst,
    size_t len)
{
    int fd = open("/dev/random", O_RDONLY, 0);
    int rtn;
    
    if(fd <= 0) {
	return EIO;
    }
    rtn = read(fd, dst, len);
    close(fd);
    if(rtn != len) {
	pkiDebug("pkinit_rand: short read\n");
	return EIO;
    }
    return 0;
}

