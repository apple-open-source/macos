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
	File:		rootCerts.h

	Contains:	embedded iSign and SSL root certs - subject name 
				and public keys

	Written by:	Doug Mitchell. 

	Copyright:	Copyright 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef	_TP_ROOT_CERTS_H_
#define _TP_ROOT_CERTS_H_

#include <Security/cssmtype.h>

#ifdef __cplusplus
extern	"C" {
#endif /* __cplusplus */

/*
 * Each one of these represents one known root cert.
 */
typedef struct {
	const CSSM_DATA * const	subjectName;	// normalized and DER-encoded
	const CSSM_DATA * const	publicKey;		// DER-encoded
	uint32 					keySize;
} tpRootCert;

extern const tpRootCert iSignRootCerts[];
extern const unsigned numiSignRootCerts;

extern const tpRootCert sslRootCerts[];
extern const unsigned numSslRootCerts;

/* These certs are shared by SSL and iSign */
extern const CSSM_DATA serverpremium_pubKey;
extern const CSSM_DATA serverpremium_subject;
extern const CSSM_DATA serverbasic_pubKey;
extern const CSSM_DATA serverbasic_subject;
extern const CSSM_DATA PCA3ss_v4_pubKey;
extern const CSSM_DATA PCA3ss_v4_subject;

#define ENABLE_APPLE_DEBUG_ROOT		0


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* _TP_ROOT_CERTS_H_ */