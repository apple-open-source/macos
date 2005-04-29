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
 * pkinit_server.h - Server side routines for PKINIT
 *
 * Created 21 May 2004 by Doug Mitchell at Apple.
 */

#ifndef _PKINIT_SERVER_H_
#define _PKINIT_SERVER_H_

#include "krb5.h"
#include "pkinit_cms.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Parse PA-PK-AS-REQ message. Optionally evaluates the message's certificate chain
 * if cert_status is non-NULL. Optionally returns various components. 
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
     * signer_cert is the full X.509 leaf cert from the incoming SignedData.
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
    krb5_data		**trustedCAs);      // krb5_data's and their content mallocd
    
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
    krb5_data		    *as_rep);		    // mallocd and RETURNED
    
#ifdef __cplusplus
}
#endif

#endif  /* _PKINIT_SERVER_H_ */
