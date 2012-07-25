/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslKeychain.h - Apple Keychain routines
 */

#ifndef	_SSL_KEYCHAIN_H_
#define _SSL_KEYCHAIN_H_


#include "sslContext.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_SSLCERTIFICATE
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
	SSLPubKey       **pubKey,		/* &ctx->signingPubKey, etc. */
	SecKeyRef		*privKeyRef,	/* &ctx->signingPrivKeyRef, etc. */
	CSSM_ALGORITHMS	*signerAlg);	/* optionally returned */
#else

OSStatus
parseIncomingCerts(
	SSLContext			*ctx,
	CFArrayRef			certs,
	CFArrayRef			*destCertChain,	/* &ctx->{localCertChain,encryptCertChain} */
	SSLPubKey			**pubKey,		/* &ctx->signingPubKey, etc. */
	SSLPrivKey			**privKeyRef,	/* &ctx->signingPrivKeyRef, etc. */
	CFIndex			*signerAlg);		/* optional */

#endif

#ifdef __cplusplus
}
#endif

#endif	/* _SSL_KEYCHAIN_H_ */
