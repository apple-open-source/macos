/*
 * Copyright (c) 2000,2002,2011,2014 Apple Inc. All Rights Reserved.
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
 * ocspUtils.h - common utilities for OCSPD
 */
#ifndef	_OCSPD_UTILS_H_
#define _OCSPD_UTILS_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <CommonCrypto/CommonDigest.h>
#include <Security/cssmtype.h>
#include <Security/SecAsn1Coder.h>
#include <CoreFoundation/CoreFoundation.h>

/*
 * Compare two CSSM_DATAs, return CSSM_TRUE if identical.
 */
CSSM_BOOL ocspdCompareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2);

/*
 * Parse a GeneralizedTime string into a CFAbsoluteTime. Returns NULL_TIME on
 * parse error. Fractional parts of a second are discarded.
 */
#define NULL_TIME	0.0

CFAbsoluteTime genTimeToCFAbsTime(
	const CSSM_DATA *strData);

/*
 * Convert CFAbsoluteTime to generalized time string, GMT format (4 digit year,
 * trailing 'Z'). Caller allocated the output which is GENERAL_TIME_STRLEN bytes plus
 * a NULL.
 */
#define GENERAL_TIME_STRLEN	15		/* NOT including trailing NULL */

void cfAbsTimeToGgenTime(
	CFAbsoluteTime		absTime,
	char				*genTime);

#define OCSPD_MAX_DIGEST_LEN		CC_SHA256_DIGEST_LENGTH

void ocspdSha1(
	const void		*data,
	CC_LONG			len,
	unsigned char	*md);			// allocd by caller, CC_SHA1_DIGEST_LENGTH bytes
void ocspdMD5(
	const void		*data,
	CC_LONG			len,
	unsigned char	*md);			// allocd by caller, CC_MD5_DIGEST_LENGTH bytes
void ocspdMD4(
	const void		*data,
	CC_LONG			len,
	unsigned char	*md);			// allocd by caller, CC_MD4_DIGEST_LENGTH bytes
void ocspdSHA256(
	const void		*data,
	CC_LONG			len,
	unsigned char	*md);			// allocd by caller, CC_SHA256_DIGEST_LENGTH bytes

/*
 * How many items in a NULL-terminated array of pointers?
 */
unsigned ocspdArraySize(
	const void **array);

/*
 * Fill out a CSSM_DATA with the subset of public key bytes from the given
 * CSSM_KEY_PTR which should be hashed to produce the issuerKeyHash field
 * of a CertID in an OCSP request.
 */
CSSM_RETURN ocspdGetPublicKeyBytes(
	SecAsn1CoderRef coder,
	CSSM_KEY_PTR publicKey,
	CSSM_DATA &publicKeyBytes); // filled out by this function


#define CFRELEASE(cf)	\
	if(cf != NULL) {	\
		CFRelease(cf);	\
	}

#ifdef	__cplusplus
}
#endif

#endif	/* _OCSPD_UTILS_H_ */
