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
 * pkinit_cert_store.h - PKINIT certificate storage/retrieval utilities
 *
 * Created 26 May 2004 by Doug Mitchell at Apple.
 */
 
#ifndef	_PKINIT_CERT_STORE_H_
#define _PKINIT_CERT_STORE_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include "krb5.h"

/*
 * Opaque reference to a machine-dependent representation of a certificate
 * which is capable of signing. On Mac OS X this is actually a SecIdentityRef.
 */
typedef void *pkinit_signing_cert_t;

/* 
 * Opaque reference to a database in which PKINIT-related certificates are stored. 
 */
typedef void *pkinit_cert_db_t;

/*
 * Obtain signing cert for specified principal. On successful (non-NULL) return, 
 * caller must eventually release the cert with pkinit_release_cert().
 */
krb5_error_code pkinit_get_client_cert(
    const char		    *principal,     // full principal string
    pkinit_signing_cert_t   *client_cert);  // RETURNED
    
/*
 * Store the specified certificate (or, more likely, some platform-dependent
 * reference to it) as the specified principal's signing cert. Passing
 * in NULL for the client_cert has the effect of deleting the relevant entry
 * in the cert storage.
 */
krb5_error_code pkinit_set_client_cert(
    const char		    *principal,     // full principal string
    pkinit_signing_cert_t   client_cert);

/* 
 * Obtain a reference to the client's cert database. Specify either principal
 * name or client_cert as obtained from pkinit_get_client_cert().
 */
krb5_error_code pkinit_get_client_cert_db(
    const char		    *principal,     // optional, full principal string
    pkinit_signing_cert_t   client_cert,    // optional, from pkinit_get_client_cert()
    pkinit_cert_db_t	    *client_cert_db);   // RETURNED

/*
 * Obtain the KDC signing cert. On successful (non-NULL) return, caller must
 * eventually release the cert with pkinit_release_cert(). Outside of an 
 * unusual test configuration this will undoubtedly fail if the caller is not 
 * running as root. 
 */
krb5_error_code pkinit_get_kdc_cert(
    pkinit_signing_cert_t   *kdc_cert);     // RETURNED

/* 
 * Obtain a reference to the KDC's cert database.
 */
krb5_error_code pkinit_get_kdc_cert_db(
    pkinit_cert_db_t   *kdc_cert_db);       // RETURNED

/*
 * Release certificate references obtained via pkinit_get_client_cert() and
 * pkinit_get_kdc_cert().
 */
extern void pkinit_release_cert(
    pkinit_signing_cert_t   cert);
    
/*
 * Release database references obtained via pkinit_get_client_cert_db() and
 * pkinit_get_kdc_cert_db().
 */
extern void pkinit_release_cert_db(
    pkinit_cert_db_t	    cert_db);
    

/* 
 * Obtain a mallocd C-string representation of a certificate's SHA1 digest. 
 * Only error is a NULL return indicating memory failure. 
 * Caller must free the returned string.
 */
char *pkinit_cert_hash_str(
    const krb5_data *cert);
    
#ifdef  __cplusplus
}
#endif

#endif  /* _PKINIT_CERT_STORE_H_ */
