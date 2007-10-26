/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All Rights Reserved.
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
	File:		 CertUI.h
	
	Description: stdio-based routines to get cert info from user. 

	Author:		 dmitch
*/

#ifndef	_CREATECERT_CERT_UI_H_
#define _CREATECERT_CERT_UI_H_

#include <Security/cssmtype.h>
#include <Security/cssmapple.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>

#ifdef	__cplusplus
extern "C" {

enum AbortException {kEOFException};

/* Dump error info. */
void showError(
	OSStatus ortn,
	const char *errStr);

/* 
 * Safe gets().
 * -- guaranteed no buffer overflow
 * -- guaranteed NULL-terminated string
 * -- handles empty string (i.e., response is just CR) properly
 */
void getString(
	char *buf,
	unsigned bufSize);

/*
 * Prompt and safe getString.
 */
void getStringWithPrompt(
	const char *prompt,			// need not end in newline
	char *buf,
	unsigned bufSize);

/* 
 * Used to interactively cook up an array of CSSM_APPLE_TP_NAME_OIDs, representing
 * a cert's RDN.
 */
typedef struct {
	const CSSM_OID	*oid;			// e.g., CSSMOID_CommonName
	const char		*description;	// e.g., "Common Name"
	const char		*example;		// e.g., "www.apple.com"
} NameOidInfo;

#define MAX_NAMES		6

/* Fill in a CSSM_APPLE_TP_NAME_OID array. */
void getNameOids(
	CSSM_APPLE_TP_NAME_OID *subjectNames,	// size MAX_NAMES mallocd by caller
	uint32 *numNames);						// RETURNED

/*
 * Free strings mallocd in getNameOids.
 */
void freeNameOids(
	CSSM_APPLE_TP_NAME_OID *subjectNames,	
	uint32 numNames);	

/* get key size and algorithm for subject key */
void getKeyParams(
	CSSM_ALGORITHMS		&keyAlg,
	uint32				&keySizeInBits);

/* given a signing key, obtain signing algorithm (int and oid format) */
OSStatus getSigAlg(
	const CSSM_KEY	*signingKey,
	CSSM_ALGORITHMS	&sigAlg,
	const CSSM_OID * &sigOid);

/*
 * Obtain key usage.
 */
 
/* these are OR-able bitfields */
typedef unsigned CU_KeyUsage;
#define kKeyUseSigning 		0x01 
#define kKeyUseEncrypting	0x02

CU_KeyUsage getKeyUsage(bool isRoot);

#endif
#ifdef	__cplusplus
}
#endif

#endif	/* _CREATECERT_CERT_UI_H_ */
