/*
 * Copyright (c) 2000-2004,2006-2008,2010-2014 Apple Inc. All Rights Reserved.
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
 * ntlmBlobPriv.c - Private routines used by NtlmGenerator module. 
 */

#include "ntlmBlobPriv.h"
#include <Security/SecBase.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/param.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <strings.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CoreFoundation/CFDate.h>
#include <Security/SecFramework.h>
#include <Security/SecRandom.h>
#include <utilities/SecCFWrappers.h>

#if		DEBUG_FIXED_CHALLENGE
/* Fixed 64-bit timestamp for sourceforge test vectors */
static unsigned char dbgStamp[] = 
{ 
	0x00, 0x90, 0xd3, 0x36, 0xb7, 0x34, 0xc3, 0x01 
};
#endif  /* DEBUG_FIXED_CHALLENGE */

// MARK: -
// MARK: Encode/Decode Routines

/* write a 64-bit word, little endian */
void appendUint64(
	CFMutableDataRef	buf,
	uint64_t			word)
{
#if 1
	unsigned char cb[8];
	OSWriteLittleInt64(cb, 0, word);
	CFDataAppendBytes(buf, cb, 8);
#else
	/* This is an alternate implementation which may or may not be faster than
	   the above. */
	CFIndex offset = CFDataGetLength(buf);
	UInt8 *bytes = CFDataGetMutableBytePtr(buf);
	CFDataIncreaseLength(buf, 8);
	OSWriteLittleInt64(bytes, offset, word);
#endif
}

/* write a 32-bit word, little endian */
void appendUint32(
	CFMutableDataRef	buf,
	uint32_t			word)
{
#if 1
	unsigned char cb[4];
	OSWriteLittleInt32(cb, 0, word);
	CFDataAppendBytes(buf, cb, 4);
#else
	/* This is an alternate implementation which may or may not be faster than
	   the above. */
	CFIndex offset = CFDataGetLength(buf);
	UInt8 *bytes = CFDataGetMutableBytePtr(buf);
	CFDataIncreaseLength(buf, 4);
	OSWriteLittleInt32(bytes, offset, word);
#endif
}

/* write a 16-bit word, little endian */
void appendUint16(
	CFMutableDataRef	buf,
	uint16_t			word)
{
	unsigned char cb[2];
	OSWriteLittleInt16(cb, 0, word);
	CFDataAppendBytes(buf, cb, 2);
}

/* 
 * Write a security buffer, providing the index into the CFData at which 
 * this security buffer's offset is located. Just before the actual data is written,
 * go back and update the offset with the start of that data using secBufOffset().
 */
void appendSecBuf(
	CFMutableDataRef	buf,
	uint16_t			len,
	CFIndex				*offsetIndex)
{
#if 1
	unsigned char cb[8];
	OSWriteLittleInt16(cb, 0, len);           /* buffer length */
	OSWriteLittleInt16(cb, 2, len);           /* buffer allocated size */
	OSWriteLittleInt32(cb, 4, 0);             /* offset is empty for now */
	CFDataAppendBytes(buf, cb, 8);         
	*offsetIndex = CFDataGetLength(buf) - 4;  /* offset will go here */
#else
	appendUint16(buf, len);					/* buffer length */
	appendUint16(buf, len);					/* buffer allocated size */
	*offsetIndex = CFDataGetLength(buf);	/* offset will go here */
	appendUint32(buf, 0);					/* but it's empty for now */
#endif
}

/*
 * Update a security buffer's offset to be the current end of data in a CFData.
 */
void secBufOffset(
	CFMutableDataRef	buf,
	CFIndex				offsetIndex)		/* obtained from appendSecBuf() */
{
	CFIndex currPos = CFDataGetLength(buf);
	unsigned char cb[4];
	OSWriteLittleInt32(cb, 0, (uint32_t)currPos);
	CFRange range = {offsetIndex, 4};
	CFDataReplaceBytes(buf, range, cb, 4);
}

/*
 * Parse/validate a security buffer. Verifies that supplied offset/length don't go
 * past end of avaialble data. Returns ptr to actual data and its length. Returns
 * NTLM_ERR_PARSE_ERR on bogus values.
 */
OSStatus ntlmParseSecBuffer(
	const unsigned char *cp,			/* start of security buffer */
	const unsigned char *bufStart,		/* start of whole msg buffer */
	unsigned bufLen,					/* # of valid bytes starting at bufStart */
	const unsigned char **data,			/* RETURNED, start of actual data */
	uint16_t *dataLen)					/* RETURNED, length of actual data */
{
	assert(cp >= bufStart);

	uint16_t secBufLen = OSReadLittleInt16(cp, 0);
	/* skip length we just parsed plus alloc size, which we don't use */
	cp += 4;
	uint32_t offset = OSReadLittleInt32(cp, 0);
	if((offset + secBufLen) > bufLen) {
		dprintf("ntlmParseSecBuffer: buf overflow\n");
		return NTLM_ERR_PARSE_ERR;
	}
	*data = bufStart + offset;
	*dataLen = secBufLen;
	return errSecSuccess;
}

// MARK: -
// MARK: CFString Converters

/*
 * Convert CFString to little-endian unicode. 
 */
OSStatus ntlmStringToLE(
	CFStringRef		pwd,
	unsigned char   **ucode,		// mallocd and RETURNED
	unsigned		*ucodeLen)		// RETURNED
{
	CFIndex len = CFStringGetLength(pwd);
    if (len > NTLM_MAX_STRING_LEN)
        return errSecAllocate;
	unsigned char *data = (unsigned char *)malloc(len * 2);
    if (data == NULL)
        return errSecAllocate;
	unsigned char *cp = data;

	CFIndex dex;
	for(dex=0; dex<len; dex++) {
		UniChar uc = CFStringGetCharacterAtIndex(pwd, dex);
		*cp++ = uc & 0xff;
		*cp++ = uc >> 8;
	}
	*ucode = data;
	*ucodeLen = (unsigned)(len * 2);

    return errSecSuccess;
}

/*
 * Convert a CFStringRef into a mallocd array of chars suitable for the specified
 * encoding. This might return an error if the string can't be converted 
 * appropriately. 
 */
OSStatus ntlmStringFlatten(
	CFStringRef str,
	bool unicode,
	unsigned char **flat,			// mallocd and RETURNED
	unsigned *flatLen)				// RETURNED
{
	if(unicode) {
		/* convert to little-endian unicode */
		return ntlmStringToLE(str, flat, flatLen);
	}
	else {
		/* convert to ASCII C string */
		CFIndex strLen = CFStringGetLength(str);
        if (strLen > NTLM_MAX_STRING_LEN)
            return errSecAllocate;

		char *cStr = (char *)malloc(strLen + 1);
		if(cStr == NULL) {
			return errSecAllocate;
		}
		if(CFStringGetCString(str, cStr, strLen + 1, kCFStringEncodingASCII)) {
			*flat = (unsigned char *)cStr;
			*flatLen = (unsigned)strLen;
			return errSecSuccess;
		}
		
		/*
		 * Well that didn't work. Try UTF8 - I don't know how a MS would behave if
		 * this portion of auth (only used for the LM response) didn't work.
		 */
		dprintf("ntlmStringFlatten: ASCII password conversion failed; trying UTF8\n");
		free(cStr);

        CFDataRef dataFromString = CFStringCreateExternalRepresentation(NULL, str, kCFStringEncodingUTF8, 0);
		if(dataFromString) {
			*flatLen = (unsigned)CFDataGetLength(dataFromString);
            *flat = malloc(*flatLen);
            if (*flat == NULL) {
                CFRelease(dataFromString);
                return errSecAllocate;
            }
            memcpy(*flat, CFDataGetBytePtr(dataFromString), *flatLen);
            CFReleaseNull(dataFromString);
			return errSecSuccess;
		}
		dprintf("lmPasswordHash: UTF8 password conversion failed\n");
        CFReleaseNull(dataFromString);
		return NTLM_ERR_PARSE_ERR;
	}
}

// MARK: -
// MARK: Machine Dependent Cruft

/* random number generator */
void ntlmRand(
	unsigned		len,
	void			*buf)				/* allocated by caller, random data RETURNED */
{
    int status;
    status=SecRandomCopyBytes(kSecRandomDefault, len, buf);
    (void)status; // Prevent warning
}

/* Obtain host name in appropriate encoding */
OSStatus ntlmHostName(
	bool unicode,
	unsigned char **flat,			// mallocd and RETURNED
	unsigned *flatLen)				// RETURNED
{
	char hostname[MAXHOSTNAMELEN];
	if(gethostname(hostname, MAXHOSTNAMELEN)) {
		#ifndef NDEBUG
		perror("gethostname");
		#endif
		return errSecInternalComponent;
	}
	size_t len = strlen(hostname);
	if(unicode) {
		/* quickie "little endian unicode" conversion */
		*flat = (unsigned char *)malloc(len * 2);
		unsigned char *cp = *flat;
		size_t dex;
		for(dex=0; dex<len; dex++) {
			*cp++ = hostname[dex];
			*cp++ = 0;
		}
		*flatLen = (unsigned)len * 2;
		return errSecSuccess;
	}
	else {
		*flat = (unsigned char *)malloc(len+1);
		*flatLen = (unsigned)len;
		memmove(*flat, hostname, len);
        flat[len] = '\0'; // ensure null terminator
		return errSecSuccess;
	}
}
	
/* 
 * Append 64-bit little-endiam timestamp to a CFData. Time is relative to 
 * January 1 1601, in tenths of a microsecond. 
 */

CFGiblisGetSingleton(CFAbsoluteTime, ntlmGetBasis, ntlmBasisAbsoluteTime, ^{
    *ntlmBasisAbsoluteTime = CFAbsoluteTimeForGregorianZuluDay(1601, 1, 1);
});

void ntlmAppendTimestamp(
	CFMutableDataRef ntlmV2Blob)
{
	#if DEBUG_FIXED_CHALLENGE
	/* Fixed 64-bit timestamp for sourceforge test vectors */
	CFDataAppendBytes(ntlmV2Blob, dbgStamp, 8);
	#else

	CFAbsoluteTime nowTime   = CFAbsoluteTimeGetCurrent();
	
	/* elapsed := time in seconds since basis */
	CFTimeInterval elapsed = nowTime - ntlmGetBasis();
	/* now in tenths of microseconds */
	elapsed *= 10000000.0;

	appendUint64(ntlmV2Blob, (uint64_t)elapsed);
	#endif
}

// MARK: -
// MARK: Crypto

/* MD4 and MD5 hash */
#define NTLM_DIGEST_LENGTH   16
void md4Hash(
	const unsigned char *data,
	unsigned			dataLen,
	unsigned char		*digest)		// caller-supplied, NTLM_DIGEST_LENGTH */
{
	CC_MD4_CTX ctx;
	CC_MD4_Init(&ctx);
	CC_MD4_Update(&ctx, data, dataLen);
	CC_MD4_Final(digest, &ctx);
}

void md5Hash(
	const unsigned char *data,
	unsigned			dataLen,
	unsigned char		*digest)		// caller-supplied, NTLM_DIGEST_LENGTH */
{
	CC_MD5_CTX ctx;
	CC_MD5_Init(&ctx);
	CC_MD5_Update(&ctx, data, dataLen);
	CC_MD5_Final(digest, &ctx);
}

/*
 * Given 7 bytes, create 8-byte DES key. Our implementation ignores the 
 * parity bit (lsb), which simplifies this somewhat. 
 */
void ntlmMakeDesKey(
	const unsigned char *inKey,			// 7 bytes
	unsigned char *outKey)				// 8 bytes
{
	outKey[0] =   inKey[0] & 0xfe;
	outKey[1] = ((inKey[0] << 7) | (inKey[1] >> 1)) & 0xfe;
	outKey[2] = ((inKey[1] << 6) | (inKey[2] >> 2)) & 0xfe;
	outKey[3] = ((inKey[2] << 5) | (inKey[3] >> 3)) & 0xfe;
	outKey[4] = ((inKey[3] << 4) | (inKey[4] >> 4)) & 0xfe;
	outKey[5] = ((inKey[4] << 3) | (inKey[5] >> 5)) & 0xfe;
	outKey[6] = ((inKey[5] << 2) | (inKey[6] >> 6)) & 0xfe;
	outKey[7] =  (inKey[6] << 1) & 0xfe;
}

/*
 * single block DES encrypt.
 * This would really benefit from a DES implementation in CommonCrypto. 
 */
OSStatus ntlmDesCrypt(
	const unsigned char *key,		// 8 bytes
	const unsigned char *inData,	// 8 bytes
	unsigned char *outData)   // 8 bytes
{
    size_t data_moved;
    return CCCrypt(kCCEncrypt, kCCAlgorithmDES, 0, key, kCCKeySizeDES, 
        NULL /*no iv, 1 block*/, inData, 1 * kCCBlockSizeDES, outData,
        1 * kCCBlockSizeDES, &data_moved);
}

/*
 * HMAC/MD5.
 */
OSStatus ntlmHmacMD5(
	const unsigned char *key,	
	unsigned			keyLen,
	const unsigned char *inData,
	unsigned			inDataLen,
	unsigned char		*mac)		// caller provided, NTLM_DIGEST_LENGTH
{
    CCHmacContext hmac_md5_context;

    CCHmacInit(&hmac_md5_context, kCCHmacAlgMD5, key, keyLen);
    CCHmacUpdate(&hmac_md5_context, inData, inDataLen);
    CCHmacFinal(&hmac_md5_context, mac);

    return errSecSuccess;
}

// MARK: -
// MARK: NTLM password and digest munging


/*
 * Calculate NTLM password hash (MD4 on a unicode password).
 */
OSStatus ntlmPasswordHash(
	CFStringRef		pwd,
	unsigned char   *digest)		// caller-supplied, NTLM_DIGEST_LENGTH
{
    OSStatus res;
	unsigned char *data;
	unsigned len;

	/* convert to little-endian unicode */
    res = ntlmStringToLE(pwd, &data, &len);
    if (res)
        return res;
	/* md4 hash of that */
	md4Hash(data, len, digest);
	free(data);

    return 0;
}

/* 
 * NTLM response: DES encrypt the challenge (or session hash) with three 
 * different keys derived from the password hash. Result is concatenation 
 * of three DES encrypts. 
 */
#define ALL_KEYS_LENGTH (3 * DES_RAW_KEY_SIZE)
OSStatus lmv2Response(
	const unsigned char *digest,		// NTLM_DIGEST_LENGTH bytes
	const unsigned char *ptext,			// challenge or session hash 
	unsigned char		*ntlmResp)		// caller-supplied NTLM_LM_RESPONSE_LEN
{
	unsigned char allKeys[ALL_KEYS_LENGTH];
	unsigned char key1[DES_KEY_SIZE], key2[DES_KEY_SIZE], key3[DES_KEY_SIZE];
	OSStatus ortn;
	
	memmove(allKeys, digest, NTLM_DIGEST_LENGTH);
	memset(allKeys + NTLM_DIGEST_LENGTH, 0, ALL_KEYS_LENGTH - NTLM_DIGEST_LENGTH);
	ntlmMakeDesKey(allKeys, key1);
	ntlmMakeDesKey(allKeys + DES_RAW_KEY_SIZE, key2);
	ntlmMakeDesKey(allKeys + (2 * DES_RAW_KEY_SIZE), key3);
	ortn = ntlmDesCrypt(key1, ptext, ntlmResp);
	if(ortn == errSecSuccess) {
		ortn = ntlmDesCrypt(key2, ptext, ntlmResp + DES_BLOCK_SIZE);
	}
	if(ortn == errSecSuccess) {
		ortn = ntlmDesCrypt(key3, ptext, ntlmResp + (2 * DES_BLOCK_SIZE));
	}
	return ortn;
}

