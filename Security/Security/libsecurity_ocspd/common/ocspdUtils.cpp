/*
 * Copyright (c) 2000,2002,2011-2012,2014 Apple Inc. All Rights Reserved.
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
 * ocspUtils.cpp - common utilities for OCSPD
 */

#include "ocspdUtils.h"
#include "ocspdDebug.h"
#include <Security/cssmerr.h>
#include <Security/keyTemplates.h>
#include <CoreFoundation/CoreFoundation.h>

/*
 * Compare two CSSM_DATAs, return CSSM_TRUE if identical.
 */
CSSM_BOOL ocspdCompareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2)
{
	if((data1 == NULL) || (data1->Data == NULL) ||
	   (data2 == NULL) || (data2->Data == NULL) ||
	   (data1->Length != data2->Length)) {
		return CSSM_FALSE;
	}
	if(data1->Length != data2->Length) {
		return CSSM_FALSE;
	}
	if(memcmp(data1->Data, data2->Data, data1->Length) == 0) {
		return CSSM_TRUE;
	}
	else {
		return CSSM_FALSE;
	}
}

/*
 * Convert a generalized time string, with a 4-digit year and no trailing
 * fractional seconds or time zone info, to a CFAbsoluteTime. Returns
 * NULL_TIME (0.0) on error.
 */
static CFAbsoluteTime parseGenTime(
	const uint8 *str,
	uint32 len)
{
	if((str == NULL) || (len == 0)) {
    	return NULL_TIME;
  	}

  	/* tolerate NULL terminated or not */
  	if(str[len - 1] == '\0') {
  		len--;
  	}
	if(len < 4) {
		return NULL_TIME;
	}
	char szTemp[5];
	CFGregorianDate greg;
	memset(&greg, 0, sizeof(greg));
	const uint8 *cp = str;

	/* YEAR */
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = *cp++;
	szTemp[3] = *cp++;
	szTemp[4] = '\0';
	len -= 4;
	greg.year = atoi(szTemp);

	/* MONTH - CFGregorianDate ranges 1..12, just like the string */
	if(len < 2) {
		return NULL_TIME;
	}
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	len -= 2;
	greg.month = atoi( szTemp );

	/* DAY - 1..31 */
	if(len < 2) {
		return NULL_TIME;
	}
	szTemp[0] = *cp++;
	szTemp[1] = *cp++;
	szTemp[2] = '\0';
	greg.day = atoi( szTemp );
	len -= 2;

	if(len >= 2) {
		/* HOUR 0..23 */
		szTemp[0] = *cp++;
		szTemp[1] = *cp++;
		szTemp[2] = '\0';
		greg.hour = atoi( szTemp );
		len -= 2;
	}
	if(len >= 2) {
		/* MINUTE 0..59 */
		szTemp[0] = *cp++;
		szTemp[1] = *cp++;
		szTemp[2] = '\0';
		greg.minute = atoi( szTemp );
		len -= 2;
	}
	if(len >= 2) {
		/* SECOND 0..59 */
		szTemp[0] = *cp++;
		szTemp[1] = *cp++;
		szTemp[2] = '\0';
		greg.second = atoi( szTemp );
		len -= 2;
	}
	return CFGregorianDateGetAbsoluteTime(greg, NULL);
}

/*
 * Parse a GeneralizedTime string into a CFAbsoluteTime. Returns NULL on parse error.
 * Fractional parts of a second are discarded.
 */
CFAbsoluteTime genTimeToCFAbsTime(
	const CSSM_DATA *strData)
{
	if((strData == NULL) || (strData->Data == NULL) || (strData->Length == 0)) {
    	return NULL_TIME;
  	}

	uint8 *timeStr = strData->Data;
	size_t timeStrLen = strData->Length;

  	/* tolerate NULL terminated or not */
  	if(timeStr[timeStrLen - 1] == '\0') {
  		timeStrLen--;
  	}

	/* start with a fresh editable copy */
	uint8 *str = (uint8 *)malloc(timeStrLen);
	uint32 strLen = 0;

	/*
	 * If there is a decimal point, strip it and all trailing digits off
	 */
	const uint8 *inCp = timeStr;
	uint8 *outCp = str;
	int foundDecimal = 0;
	int minutesOffset = 0;
	int hoursOffset = 0;
	bool minusOffset = false;
	bool isGMT = false;
	size_t toGo = timeStrLen;

	do {
		if(*inCp == '.') {
			if(foundDecimal) {
				/* only legal once */ {
					free(str);
					return NULL_TIME;
				}
			}
			foundDecimal++;

			/* skip the decimal point... */
			inCp++;
			toGo--;
			if(toGo == 0) {
				/* all done */
				break;
			}
			/* then all subsequent contiguous digits */
			while(isdigit(*inCp) && (toGo != 0)) {
				inCp++;
				toGo--;
			}
		}	/* decimal point processing */
		else if((*inCp == '+') || (*inCp == '-')) {
			/* Time zone offset - handle 2 or 4 chars */
			if((toGo != 2) & (toGo != 4)) {
				free(str);
				return NULL_TIME;
			}
			if(*inCp == '-') {
				minusOffset = true;
			}
			inCp++;
			hoursOffset = (10 * (inCp[0] - '0')) + (inCp[1] - '0');
			toGo -= 2;
			if(toGo) {
				minutesOffset = (10 * (inCp[0] - '0')) + (inCp[1] - '0');
				toGo -= 2;
			}
		}
		else {
			*outCp++ = *inCp++;
			strLen++;
			toGo--;
		}
	} while(toGo != 0);

	if(str[strLen - 1] == 'Z') {
		isGMT = true;
		strLen--;
	}

	CFAbsoluteTime absTime;
	absTime = parseGenTime(str, strLen);
	free(str);
	if(absTime == NULL_TIME) {
		return NULL_TIME;
	}

	/* post processing needed? */
	if(isGMT) {
		/* Nope, string was in GMT */
		return absTime;
	}
	if((minutesOffset != 0) || (hoursOffset != 0)) {
		/* string contained explicit offset from GMT */
		if(minusOffset) {
			absTime -= (minutesOffset * 60);
			absTime -= (hoursOffset * 3600);
		}
		else {
			absTime += (minutesOffset * 60);
			absTime += (hoursOffset * 3600);
		}
	}
	else {
		/* implciit offset = local */
		CFTimeInterval tzDelta;
		CFTimeZoneRef localZone = CFTimeZoneCopySystem();
		tzDelta = CFTimeZoneGetSecondsFromGMT (localZone, CFAbsoluteTimeGetCurrent());
		CFRelease(localZone);
		absTime += tzDelta;
	}
	return absTime;
}

/*
 * Convert CFAbsoluteTime to generalized time string, GMT format (4 digit year,
 * trailing 'Z'). Caller allocated the output which is GENERAL_TIME_STRLEN+1 bytes.
 */
void cfAbsTimeToGgenTime(
	CFAbsoluteTime		absTime,
	char				*genTime)
{
	/* time zone = GMT */
	CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0.0);
	CFGregorianDate greg = CFAbsoluteTimeGetGregorianDate(absTime, tz);
	int seconds = (int)greg.second;
	sprintf(genTime, "%04d%02d%02d%02d%02d%02dZ",
				(int)greg.year, greg.month, greg.day, greg.hour,
				greg.minute, seconds);
}

void ocspdSha1(
	const void		*data,
	CC_LONG			len,
	unsigned char	*md)		// allocd by caller, CC_SHA1_DIGEST_LENGTH bytes
{
	CC_SHA1_CTX ctx;
	CC_SHA1_Init(&ctx);
	CC_SHA1_Update(&ctx, data, len);
	CC_SHA1_Final(md, &ctx);
}

void ocspdMD5(
	const void		*data,
	CC_LONG			len,
	unsigned char	*md)		// allocd by caller, CC_MD5_DIGEST_LENGTH bytes
{
	CC_MD5_CTX ctx;
	CC_MD5_Init(&ctx);
	CC_MD5_Update(&ctx, data, len);
	CC_MD5_Final(md, &ctx);
}

void ocspdMD4(
	const void		*data,
	CC_LONG			len,
	unsigned char	*md)		// allocd by caller, CC_MD4_DIGEST_LENGTH bytes
{
	CC_MD4_CTX ctx;
	CC_MD4_Init(&ctx);
	CC_MD4_Update(&ctx, data, len);
	CC_MD4_Final(md, &ctx);
}

void ocspdSHA256(
	const void		*data,
	CC_LONG			len,
	unsigned char	*md)		// allocd by caller, CC_SHA256_DIGEST_LENGTH bytes
{
	CC_SHA256_CTX ctx;
	CC_SHA256_Init(&ctx);
	CC_SHA256_Update(&ctx, data, len);
	CC_SHA256_Final(md, &ctx);
}

/*
 * How many items in a NULL-terminated array of pointers?
 */
unsigned ocspdArraySize(
	const void **array)
{
    unsigned count = 0;
    if (array) {
		while (*array++) {
			count++;
		}
    }
    return count;
}

/* Fill out a CSSM_DATA with the subset of public key bytes from the given
 * CSSM_KEY_PTR which should be hashed to produce the issuerKeyHash field
 * of a CertID in an OCSP request.
 *
 * For RSA keys, this simply copies the input key pointer and length.
 * For EC keys, we need to further deconstruct the SubjectPublicKeyInfo
 * to obtain the key bytes (i.e. curve point) for hashing.
 *
 * Returns CSSM_OK on success, or non-zero error if the bytes could not
 * be retrieved.
 */
CSSM_RETURN ocspdGetPublicKeyBytes(
	SecAsn1CoderRef coder,		// optional
	CSSM_KEY_PTR publicKey,		// input public key
	CSSM_DATA &publicKeyBytes)	// filled in by this function
{
	CSSM_RETURN crtn = CSSM_OK;
	SecAsn1CoderRef _coder = NULL;

	if(publicKey == NULL) {
		crtn = CSSMERR_CSP_INVALID_KEY_POINTER;
		goto exit;
	}

	if(coder == NULL) {
		crtn = SecAsn1CoderCreate(&_coder);
		if(crtn) {
			goto exit;
		}
		coder = _coder;
	}

	publicKeyBytes.Length = publicKey->KeyData.Length;
	publicKeyBytes.Data = publicKey->KeyData.Data;

	if(publicKey->KeyHeader.AlgorithmId == CSSM_ALGID_ECDSA) {
		/*
		 * For an EC key, publicKey->KeyData is a SubjectPublicKeyInfo
		 * ASN.1 sequence that includes the algorithm identifier.
		 * We only want to return the bit string portion of the key here.
		 */
		SecAsn1PubKeyInfo pkinfo;
		memset(&pkinfo, 0, sizeof(pkinfo));
		if(SecAsn1Decode(coder,
			publicKey->KeyData.Data,
			publicKey->KeyData.Length,
			kSecAsn1SubjectPublicKeyInfoTemplate,
			&pkinfo) == 0) {
			if(pkinfo.subjectPublicKey.Length &&
			   pkinfo.subjectPublicKey.Data) {
				publicKeyBytes.Length = pkinfo.subjectPublicKey.Length >> 3;
				publicKeyBytes.Data = pkinfo.subjectPublicKey.Data;
				/*
				 * Important: if we allocated the SecAsn1Coder, the memory
				 * being pointed to by pkinfo.subjectPublicKey.Data will be
				 * deallocated when the coder is released below. We want to
				 * point to the identical data inside the caller's public key,
				 * now that the decoder has identified it for us.
				 */
				if(publicKeyBytes.Length <= publicKey->KeyData.Length) {
					publicKeyBytes.Data = (uint8*)((uintptr_t)publicKey->KeyData.Data +
						(publicKey->KeyData.Length - publicKeyBytes.Length));
					goto exit;
				}
				/* intentional fallthrough to error exit */
			}
			ocspdErrorLog("ocspdGetPublicKeyBytes: invalid SecAsn1PubKeyInfo\n");
			crtn = CSSMERR_CSP_INVALID_KEY_POINTER;
		}
		else {
			/* Unable to decode using kSecAsn1SubjectPublicKeyInfoTemplate.
			 * This may or may not be an error; just return the unchanged key.
			 */
			ocspdErrorLog("ocspdGetPublicKeyBytes: unable to decode SubjectPublicKeyInfo\n");
		}
	}

exit:
	if(_coder) {
		SecAsn1CoderRelease(_coder);
	}
	return crtn;
}
