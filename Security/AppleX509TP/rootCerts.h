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

	Contains:	Interface to local cache of system-wide trusted root certs

	Written by:	Doug Mitchell. 

	Copyright:	Copyright 1999 by Apple Computer, Inc., all rights reserved.

*/

#ifndef	_TP_ROOT_CERTS_H_
#define _TP_ROOT_CERTS_H_

/*
 * As of 3/18/02, use of the built-in root certs is disabled by default. 
 * Their use is enabled at in CSSM_TP_CertGroupVerify by the use of the 
 * CSSM_TP_USE_INTERNAL_ROOT_CERTS bit in 
 * CSSM_APPLE_TP_ACTION_DATA.ActionFlags.  The presence of the root certs 
 * at all (at compile time) is controlled TP_ROOT_CERT_ENABLE.
 */
#define TP_ROOT_CERT_ENABLE		0

#if		TP_ROOT_CERT_ENABLE

#include <Security/cssmtype.h>
#include <Security/globalizer.h>
#include <Security/threading.h>
#include "TPCertInfo.h"

/*
 * Each one of these represents one known root cert.
 */
typedef struct {
	CSSM_DATA 	subjectName;	// normalized and DER-encoded
	CSSM_DATA 	publicKey;		// DER-encoded
	uint32 		keySize;
} tpRootCert;

/* One of these per process which caches the roots in tpRootCert format */
class TPRootStore
{
public:
	TPRootStore() : mRootCerts(NULL), mNumRootCerts(0) { }
	~TPRootStore();
	const tpRootCert *rootCerts(
		CSSM_CL_HANDLE clHand,
		unsigned &numRootCerts);
	static ModuleNexus<TPRootStore> tpGlobalRoots;
	
private:
	tpRootCert *mRootCerts;
	unsigned mNumRootCerts;
	Mutex mLock;
};


/*
 * Compare a root cert to a list of known embedded roots.
 */
extern "C" {

CSSM_BOOL tp_isKnownRootCert(
	TPCertInfo		*rootCert,			// raw cert to compare
	CSSM_CL_HANDLE	clHand);
	
CSSM_BOOL tp_verifyWithKnownRoots(
	CSSM_CL_HANDLE	clHand, 
	CSSM_CSP_HANDLE	cspHand, 
	TPCertInfo 		*certToVfy);		// last in chain, not root

}

#endif	/* TP_ROOT_CERT_ENABLE */

#endif	/* _TP_ROOT_CERTS_H_ */
