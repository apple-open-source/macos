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
 * pkinit_asn1.h - ASN.1 encode/decode routines for PKINIT
 *
 * Created 18 May 2004 by Doug Mitchell.
 */
 
#ifndef	_PKINIT_ASN1_H_
#define _PKINIT_ASN1_H_

#include "krb5.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Encode and decode AuthPack, public key version (no Diffie-Hellman components).
 */
krb5_error_code pkinit_auth_pack_encode(
    krb5_timestamp      ctime,      
    krb5_ui_4		cusec,		// microseconds
    krb5_ui_4		nonce,
    const krb5_checksum *checksum,
    krb5_data		*auth_pack);    // mallocd and RETURNED
    
/* all returned values are optional - pass NULL if you don't want them */
krb5_error_code pkinit_auth_pack_decode(
    const krb5_data	*auth_pack,     // DER encoded
    krb5_timestamp      *ctime,		// RETURNED
    krb5_ui_4		*cusec,		// microseconds, RETURNED
    krb5_ui_4		*nonce,		// RETURNED
    krb5_checksum       *checksum);     // contents mallocd and RETURNED
    
/*
 * Given DER-encoded issuer and serial number, create an encoded 
 * IssuerAndSerialNumber.
 */
krb5_error_code pkinit_issuer_serial_encode(
    const krb5_data *issuer,		    // DER encoded
    const krb5_data *serial_num,
    krb5_data       *issuer_and_serial);    // content mallocd and RETURNED

/*
 * Decode IssuerAndSerialNumber.
 */
krb5_error_code pkinit_issuer_serial_decode(
    const krb5_data *issuer_and_serial,     // DER encoded
    krb5_data       *issuer,		    // DER encoded, RETURNED
    krb5_data       *serial_num);	    // RETURNED

/*
 * Encode a TrustedCA.
 * Exactly one of {ca_name, issuer_serial} must be present; ca_name is DER-encoded
 * on entry. 
 */
krb5_error_code pkinit_trusted_ca_encode(
    const krb5_data *ca_name,		
    const krb5_data *issuer_serial,
    krb5_data       *trusted_ca);		// mallocd and RETURNED

/*
 * Decode TrustedCA. A properly encoded TrustedCA will contain exactly one of 
 * of {ca_name, issuer_serial}. The ca_name is returned in DER-encoded form.
 */
krb5_error_code pkinit_trusted_ca_decode(
    const krb5_data *trusted_ca,
    krb5_data       *ca_name,		// content optionally mallocd and RETURNED
    krb5_data       *issuer_serial);	// ditto

/*
 * Top-level encode for PA-PK-AS-REQ.  
 */
krb5_error_code pkinit_pa_pk_as_req_encode(
    const krb5_data *signed_auth_pack,		// DER encoded ContentInfo
    unsigned	    num_trusted_certifiers,     // sizeof trusted_certifiers
    const krb5_data *trusted_certifiers,	// array of DER-encoded TrustedCAs, 
						//    optional
    const krb5_data *kdc_cert,			// DER encoded issuer/serial, optional
    const krb5_data *encryption_cert,		// DER encoded issuer/serial, optional
    krb5_data       *pa_pk_as_req);		// mallocd and RETURNED

/*
 * Top-level decode for PA-PK-AS-REQ. Does not perform cert verification on the 
 * ContentInfo; that is returned in DER-encoded form and processed elsewhere.
 */
krb5_error_code pkinit_pa_pk_as_req_decode(
    const krb5_data *pa_pk_as_req,
    krb5_data *signed_auth_pack,	    // DER encoded ContentInfo, RETURNED
    /* 
     * Remainder are optionally RETURNED (specify NULL for pointers to 
     * items you're not interested in).
     */
    unsigned *num_trusted_certifiers,       // sizeof trusted_certifiers
    krb5_data **trusted_certifiers,	    // mallocd array of DER-encoded TrustedCAs
    krb5_data *kdc_cert,		    // DER encoded issuer/serial
    krb5_data *encryption_cert);	    // DER encoded issuer/serial

/* 
 * Encode a ReplyKeyPack. The result is used as the Content of a SignedData.
 */
krb5_error_code pkinit_reply_key_pack_encode(
    const krb5_keyblock *key_block,
    krb5_ui_4		nonce,
    krb5_data		*reply_key_pack);   // mallocd and RETURNED

/* 
 * Decode a ReplyKeyPack.
 */
krb5_error_code pkinit_reply_key_pack_decode(
    const krb5_data	*reply_key_pack,
    krb5_keyblock       *key_block,	    // RETURNED
    krb5_ui_4		*nonce);	    // RETURNED

/* 
 * Encode a KRB5-PA-PK-AS-REP.
 */
krb5_error_code pkinit_pa_pk_as_rep_encode(
    const krb5_data     *dh_signed_data, 
    const krb5_data     *enc_key_pack, 
    krb5_data		*pa_pk_as_rep);	    // mallocd and RETURNED

/* 
 * Decode a KRB5-PA-PK-AS-REP.
 */
krb5_error_code pkinit_pa_pk_as_rep_decode(
    const krb5_data     *pa_pk_as_rep,
    krb5_data		*dh_signed_data, 
    krb5_data		*enc_key_pack);

#ifdef __cplusplus
}
#endif

#endif	/* _PKINIT_ASN1_H_ */

