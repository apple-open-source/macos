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
#include "pkinit_cert_store.h"      /* for krb5_pkinit_signing_cert_t */
#include "pkinit_asn1.h"	    /* for krb5int_algorithm_id */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Define ContentType for a SignedData and EnvelopedData.
 */
enum {
    /* normal CMS ContentTypes */
    ECT_Data,	
    ECT_SignedData,
    ECT_EnvelopedData,
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
    ECT_PkReplyKeyKata,
    
    /*
     * Other - i.e., unrecognized ContentType on decode. 
     */
    ECT_Other
};
typedef krb5_int32 krb5int_cms_content_type;

/*
 * Result of certificate and signature verification.
 */
enum {
    pki_cs_good = 0,
    pki_not_signed,	    /* message not signed */
    pki_not_evaluated,	    /* signed, but not evaluated per caller request */
    /* remainder imply good signature on the message proper, i.e., these
     * are all certificate errors. */
    pki_cs_sig_verify_fail, /* signature verification failed */
    pki_cs_bad_leaf,	    /* leaf/subject cert itself is plain bad */
    pki_cs_no_root,	    /* looks good but not verifiable to any root */
    pki_cs_unknown_root,    /* verified to root we don't recognize */
    pki_cs_expired,	    /* expired */
    pki_cs_not_valid_yet,   /* cert not valid yet */
    pki_cs_revoked,	    /* revoked via CRL or OCSP */
    pki_cs_untrusted,	    /* marked by user as untrusted */
    pki_bad_cms,	    /* CMS Format precluded verification */
    pki_bad_key_use,	    /* Bad ExtendedKeyUse or KeyUsage extension */
    pki_bad_digest,	    /* unacceptable CMS digest algorithm */
    pki_cs_other_err	    /* other cert verify error */
};
typedef krb5_int32 krb5int_cert_sig_status;

/*
 * Create a CMS message: either encrypted (EnvelopedData), signed 
 * (SignedData), or both (EnvelopedData(SignedData(content)).
 *
 * The message is signed iff signing_cert is non-NULL.
 * The message is encrypted iff recip_cert is non-NULL.
 *
 * The content_type argument specifies to the eContentType
 * for a SignedData's EncapsulatedContentInfo; it's ignored
 * if the message is not to be signed.
 *
 * The cms_types argument optionally specifies a list, in order
 * of decreasing preference, of CMS algorithms to use in the 
 * creation of the CMS message. 
 */
krb5_error_code krb5int_pkinit_create_cms_msg(
    const krb5_data		*content,	/* Content */
    krb5_pkinit_signing_cert_t	signing_cert,	/* optional: signed by this cert */
    const krb5_data		*recip_cert,	/* optional: encrypted with this cert */
    krb5int_cms_content_type	content_type,   /* OID for EncapsulatedData */
    krb5_ui_4			num_cms_types,	/* optional */
    const krb5int_algorithm_id	*cms_types,	/* optional */
    krb5_data			*content_info); /* contents mallocd and RETURNED */

/*
 * Parse a ContentInfo as best we can. All returned fields are optional - 
 * pass NULL for values you don't need. 
 *
 * If signer_cert_status is NULL on entry, NO signature or cert evaluation 
 * will be performed. 
 *
 * The is_client_msg argument indicates whether the CMS message originated
 * from the client (TRUE) or server (FALSE) and may be used in platform-
 * dependent certificate evaluation. 
 *
 * Note that signature and certificate verification errors do NOT cause
 * this routine itself to return an error; caller is reponsible for 
 * handling such errors per the signer_cert_status out parameter. 
 */
krb5_error_code krb5int_pkinit_parse_cms_msg(
    const krb5_data     *content_info,
    krb5_pkinit_cert_db_t cert_db,		/* may be required for SignedData */
    krb5_boolean	is_client_msg,		/* TRUE : msg is from client */
    krb5_boolean	*is_signed,		/* RETURNED */
    krb5_boolean	*is_encrypted,		/* RETURNED */
    krb5_data		*raw_data,		/* RETURNED */
    krb5int_cms_content_type *inner_content_type,/* Returned, ContentType of */
						/*    EncapsulatedData if */
						/*    *is_signed true */
    /* returned for type SignedData only */
    krb5_data		*signer_cert,		/* RETURNED */
    krb5int_cert_sig_status *signer_cert_status,/* RETURNED */
    unsigned		*num_all_certs,		/* size of *all_certs RETURNED */
    krb5_data		**all_certs);		/* entire cert chain RETURNED */

/* 
 * An AuthPack contains an optional set of AlgorithmIdentifiers 
 * which define the CMS algorithms supported by the client, in 
 * order of decreasing preference. 
 *
 * krb5int_pkinit_get_cms_types() is a CMS-implementation-dependent
 * function returning supported CMS algorithms in the form of a
 * pointer and a length suitable for passing to 
 * krb5int_pkinit_auth_pack_encode. If no preference is to be expressed,
 * this function returns NULL/0 (without returning a nonzero krb5_error_code).
 *
 * krb5int_pkinit_free_cms_types() frees the pointer obtained
 * from krb5int_pkinit_get_cms_types() as necessary.
 */
krb5_error_code krb5int_pkinit_get_cms_types(
    krb5int_algorithm_id    **supported_cms_types,	/* RETURNED */
    krb5_ui_4		    *num_supported_cms_types);	/* RETURNED */
    
krb5_error_code krb5int_pkinit_free_cms_types(
    krb5int_algorithm_id    *supported_cms_types,
    krb5_ui_4		    num_supported_cms_types);

#ifdef __cplusplus
}
#endif

#endif  /* _PKINIT_CMS_H_ */