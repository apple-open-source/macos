/*
	File:		 CertUI.h
	
	Description: stdio-based routines to get cert info from user. 

	Author:		dmitch

	Copyright: 	© Copyright 2002 Apple Computer, Inc. All rights reserved.
	
	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple 
	            Computer, Inc. ("Apple") in consideration of your agreement to 
				the following terms, and your use, installation, modification 
				or redistribution of this Apple software constitutes acceptance 
				of these terms.  If you do not agree with these terms, please 
				do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following 
				terms, and subject to these terms, Apple grants you a personal, 
				non-exclusive license, under Apple's copyrights in this 
				original Apple software (the "Apple Software"), to use, 
				reproduce, modify and redistribute the Apple Software, with 
				or without modifications, in source and/or binary forms; 
				provided that if you redistribute the Apple Software in 
				its entirety and without modifications, you must retain
				this notice and the following text and disclaimers in all 
				such redistributions of the Apple Software.  Neither the 
				name, trademarks, service marks or logos of Apple Computer, 
				Inc. may be used to endorse or promote products derived from the
				Apple Software without specific prior written permission from 
				Apple.  Except as expressly stated in this notice, no other 
				rights or licenses, express or implied, are granted by Apple 
				herein, including but not limited to any patent rights that
				may be infringed by your derivative works or by other works 
				in which the Apple Software may be incorporated.

				The Apple Software is provided by Apple on an "AS IS" basis.  
				APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING 
				WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
				MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, 
				REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE 
				OR IN COMBINATION WITH YOUR PRODUCTS.

				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, 
				INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
				LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
				LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
				ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION 
				AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED 
				AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING 
				NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE 
				HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef	_CREATECERT_CERT_UI_H_
#define _CREATECERT_CERT_UI_H_

#include <Security/cssmtype.h>
#include <Security/cssmapple.h>

#ifdef	__cplusplus
extern "C" {

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

#define MAX_NAMES		5

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
