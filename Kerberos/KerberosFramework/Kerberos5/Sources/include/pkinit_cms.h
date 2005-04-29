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
 * pkinit_apple_cms.h - CMS encode/decode routines, Mac OS X version
 *
 * Created 19 May 2004 by Doug Mitchell at Apple.
 */

#ifndef _PKINIT_CMS_H_
#define _PKINIT_CMS_H_

#include "krb5.h"
#include "pkinit_cert_store.h"      /* for pkinit_signing_cert_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Define ContentType for a SignedData and EnvelopedData.
 */
typedef enum {
    /* normal CMS ContentTypes */
    ECT_Data,	
    ECT_SignedData,
    ECT_EnvenopedData,
    ECT_EncryptedData,
    
    /*
     * For SignedAuthPack
     * pkauthdata: { iso (1) org (3) dod (6) internet (1)
     *               security (5) kerberosv5 (2) pkinit (3) pkauthdata (1)}
     */
    ECT_PkAuthData,
    
    /*
     * For ReplyKeyPack
     * pkrkeydata: { iso (1) org (3) dod (6) internet (1)
     *               security (5) kerberosv5 (2) pkinit (3) pkrkeydata (3) }
     */
    ECT_PkReplyKeyKata
    /* etc. */
} PKI_ContentType;

/*
 * Result of certificate and signature verification.
 */
typedef enum {
    pki_cs_good = 0,
    pki_cs_sig_verify_fail, // signature verification failed
    pki_cs_bad_leaf,	    // leaf/subject cert itself is plain bad
    pki_cs_no_root,	    // looks good but not verifiable to any root
    pki_cs_unknown_root,    // verified to root we don't recognize
    pki_cs_expired,	    // expired
    pki_cs_not_valid_yet,   // cert not valid yet
    pki_cs_revoked,	    // revoked via CRL or OCSP
    pki_cs_untrusted,	    // marked by user as untrusted
    pki_cs_other_err	    // other cert verify error
} pki_cert_sig_status;

/*
 * Create a ContentInfo, Type SignedData. 
 * NOTE: in the current (Apple) implementation, specifying FALSE for include_cert
 * does indeed result in a SignedData with no (optional) CertificateSet in the 
 * SignedData; however, when parsing the SignedData with pkinit_parse_content_info(),
 * you can still get the signer cert, which the CMS library fetches and verifies
 * based on the SignerInfo. 
 */
krb5_error_code pkinit_create_signed_data(
    const krb5_data	    *to_be_signed,	// Content
    pkinit_signing_cert_t   signing_cert,	// to be signed by this cert
    krb5_boolean	    include_cert,	// TRUE --> include signing_cert in 
						//     SignerInfo
    PKI_ContentType	    content_type,       // OID for EncapsulatedData
    krb5_data		    *content_info);     // contents mallocd and RETURNED

/*
 * Create a ContentInfo, Type EnvelopedData. 
 */
krb5_error_code pkinit_create_envel_data(
    const krb5_data	*content,	    // Content
    const krb5_data	*recip_cert,	    // to be encrypted with this cert
    PKI_ContentType     content_type,       // OID for EncryptedContentInfo
    krb5_data		*content_info);     // contents mallocd and RETURNED

/*
 * Parse a ContentInfo as best we can. All return fields are optional.
 * If signer_cert_status is NULL on entry, NO signature or cert evaluation 
 * will be performed. 
 */
krb5_error_code pkinit_parse_content_info(
    const krb5_data     *content_info,
    pkinit_cert_db_t    cert_db,		// may be required for SignedData
    krb5_boolean	*is_signed,		// RETURNED
    krb5_boolean	*is_encrypted,		// RETURNED
    krb5_data		*raw_data,		// RETURNED
    PKI_ContentType     *inner_content_type,    // Returned, ContentType of
						//    EncapsulatedData or
						//    EncryptedContentInfo
						
    /* returned for type SignedData only */
    krb5_data		*signer_cert,		// RETURNED
    pki_cert_sig_status *signer_cert_status,    // RETURNED 
    unsigned		*num_all_certs,		// size of *all_certs RETURNED
    krb5_data		**all_certs);		// entire cert chain RETURNED

#ifdef __cplusplus
}
#endif

#endif  /* _PKINIT_CMS_H_ */
