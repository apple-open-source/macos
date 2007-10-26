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
 * pkinit_client.h - Client side routines for PKINIT
 *
 * Created 20 May 2004 by Doug Mitchell at Apple.
 */

#ifndef _PKINIT_CLIENT_H_
#define _PKINIT_CLIENT_H_

#include "krb5.h"
#include "pkinit_cms.h"
#include "pkinit_cert_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create a PA-PK-AS-REQ message.
 */
krb5_error_code krb5int_pkinit_as_req_create(
    krb5_context		context,
    krb5_timestamp		kctime,      
    krb5_int32			cusec,		/* microseconds */
    krb5_ui_4			nonce,
    const krb5_checksum		*cksum,
    krb5_pkinit_signing_cert_t	client_cert,	/* required! */
    
    /* 
     * trusted_CAs correponds to PA-PK-AS-REQ.trustedCertifiers.
     * Expressed here as an optional list of DER-encoded certs. 
     */
    const krb5_data		*trusted_CAs,	
    krb5_ui_4			num_trusted_CAs,
    
    /* optional PA-PK-AS-REQ.kdcPkId, expressed here as a 
     * DER-encoded cert */
    const krb5_data		*kdc_cert,	
    krb5_data			*as_req);	/* mallocd and RETURNED */

/*
 * Parse PA-PK-AS-REP message. Optionally evaluates the message's certificate chain. 
 * Optionally returns various components. 
 */
krb5_error_code krb5int_pkinit_as_rep_parse(
    krb5_context		context,
    const krb5_data		*as_rep,
    krb5_pkinit_signing_cert_t   client_cert,	/* required for decryption */
    krb5_keyblock		*key_block,     /* RETURNED */
    krb5_checksum		*checksum,	/* checksum of corresponding AS-REQ */
						/*   contents mallocd and RETURNED */
    krb5int_cert_sig_status	*cert_status,   /* RETURNED */

    /*
     * Cert fields, all optionally RETURNED.
     *
     * signer_cert is the DER-encoded leaf cert from the incoming SignedData.
     * all_certs is an array of all of the certs in the incoming SignedData,
     *    in full DER-encoded form. 
     */
    krb5_data		    *signer_cert,   /* content mallocd */
    unsigned		    *num_all_certs, /* sizeof *all_certs */
    krb5_data		    **all_certs);   /* krb5_data's and their content mallocd */

#ifdef __cplusplus
}
#endif

#endif  /* _PKINIT_CLIENT_H_ */