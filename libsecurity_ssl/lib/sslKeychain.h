/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		sslKeychain.h

	Contains:	Apple Keychain routines

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef	_SSL_KEYCHAIN_H_
#define _SSL_KEYCHAIN_H_


#include "sslContext.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Given an array of certs (as KCItemRefs) and a destination
 * SSLCertificate:
 *
 * -- free destCerts if we have any
 * -- Get raw cert data, convert to array of SSLCertificates in *destCert 
 * -- get pub, priv keys from certRef[0], store in *pubKey, *privKey
 * -- validate cert chain
 *
 */
OSStatus 
parseIncomingCerts(
	SSLContext		*ctx,
	CFArrayRef		certs,
	SSLCertificate	**destCert,		/* &ctx->{localCert,encryptCert} */
	CSSM_KEY_PTR	*pubKey,		/* &ctx->signingPubKey, etc. */
	SecKeyRef		*privKeyRef,	/* &ctx->signingPrivKeyRef, etc. */
	CSSM_ALGORITHMS	*signerAlg);	/* optionally returned */
	
#ifdef __cplusplus
}
#endif

#endif	/* _SSL_KEYCHAIN_H_ */
