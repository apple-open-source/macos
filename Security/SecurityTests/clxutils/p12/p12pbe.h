/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
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
 * p12pbe.h - PKCS12 PBE routine. App space reference version.
 */
 
#ifndef	_P12_PBE_H_
#define _P12_PBE_H_

#include <Security/cssmtype.h>

#ifdef __cplusplus
extern "C" {
#endif

/* specify which flavor of bits to generate */
typedef enum {
	PBE_ID_Key 	= 1,
	PBE_ID_IV  	= 2,
	PBE_ID_Mac	= 3
} P12_PBE_ID;	

/*
 * PBE generator per PKCS12 v.1 section B.2.
 */
CSSM_RETURN p12PbeGen_app(
	const CSSM_DATA	&pwd,		// unicode, double null terminated
	const unsigned char *salt,
	unsigned saltLen,
	unsigned iterCount,
	P12_PBE_ID pbeId,
	CSSM_ALGORITHMS hashAlg,	// MS5 or SHA1 only
	CSSM_CSP_HANDLE cspHand,
	
	/* result goes here, mallocd by caller */
	unsigned char *outbuf,	
	unsigned outbufLen);
	
#ifdef __cplusplus
}
#endif

#endif	/* _P12_PBE_H_ */

