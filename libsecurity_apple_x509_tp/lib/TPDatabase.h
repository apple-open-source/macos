/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
 * TPDatabase.h - TP's DL/DB access functions.
 *
 * Created 10/9/2002 by Doug Mitchell.
 */
 
#ifndef	_TP_DATABASE_H_
#define _TP_DATABASE_H_

#include <Security/cssmtype.h>
#include <security_utilities/alloc.h>
#include "TPCertInfo.h"

#ifdef	__cplusplus
extern "C" {
#endif

TPCertInfo *tpDbFindIssuerCert(
	Allocator				&alloc,
	CSSM_CL_HANDLE			clHand,
	CSSM_CSP_HANDLE			cspHand,
	const TPClItemInfo		*subjectItem,
	const CSSM_DL_DB_LIST	*dbList,
	const char 				*verifyTime,		// may be NULL
	bool					&partialIssuerKey);	// RETURNED

/*
 * Search a list of DBs for a CRL from the specified issuer and (optional)  
 * TPVerifyContext.verifyTime. 
 * Just a boolean return - we found it, or not. If we did, we return a
 * TPCrlInfo which has been verified with the specified TPVerifyContext.
 */
class TPCrlInfo;
class TPVerifyContext;

TPCrlInfo *tpDbFindIssuerCrl(
	TPVerifyContext		&vfyCtx,
	const CSSM_DATA		&issuer,
	TPCertInfo			&forCert);

#if WRITE_FETCHED_CRLS_TO_DB
/*
 * Store a CRL in a DLDB.
 */
CSSM_RETURN tpDbStoreCrl(
	TPCrlInfo			&crl,
	CSSM_DL_DB_HANDLE	&dlDb);

#endif	/* WRITE_FETCHED_CRLS_TO_DB */

#ifdef	__cplusplus
}
#endif

#endif	/* _TP_DATABASE_H_ */
