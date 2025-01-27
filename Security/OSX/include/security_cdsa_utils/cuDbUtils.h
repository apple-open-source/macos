/*
 * Copyright (c) 2002-2003,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/*
	File:		 cuDbUtils.h
	
	Description: CDSA DB access utilities

	Author:		 dmitch
*/

#ifndef	_CU_DB_UTILS_H_
#define _CU_DB_UTILS_H_

#include <Security/cssm.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Add a certificate to an open DLDB.
 */
CSSM_RETURN cuAddCertToDb(
	CSSM_DL_DB_HANDLE	dlDbHand,
	const CSSM_DATA		*cert,
	CSSM_CERT_TYPE		certType,
	CSSM_CERT_ENCODING	certEncoding,
	const char			*printName,			// C string
	const CSSM_DATA		*publicKeyHash); 	// ??

/*
 * Add a CRL to an open DL/DB.
 */
CSSM_RETURN cuAddCrlToDb(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_CL_HANDLE		clHand,
	const CSSM_DATA		*crl,
	const CSSM_DATA		*URI);

/*
 * Search DB for all records of type CRL or cert, calling appropriate
 * parse/print routine for each record. 
 */ 
CSSM_RETURN cuDumpCrlsCerts(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_CL_HANDLE		clHand,
	CSSM_BOOL			isCert,
	unsigned			&numItems,		// returned
	CSSM_BOOL			verbose);

#ifdef	__cplusplus
}
#endif

#endif	/* _CU_DB_UTILS_H_ */
